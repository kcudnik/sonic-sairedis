#include "BoostSHMChannel.h"

#include "sairediscommon.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/select.h"

#include <boost/interprocess/sync/named_mutex.hpp>

using namespace sairedis;

/**
 * @brief Get response timeout in milliseconds.
 */
#define BOOST_SHM_GETRESPONSE_TIMEOUT_MS (2*60*1000)

BoostSHMChannel::BoostSHMChannel(
        _In_ uint32_t globalContext,
        _In_ Channel::Callback callback):
    Channel(callback),
    m_globalContext(globalContext),
    m_data(nullptr)
{
    SWSS_LOG_ENTER();

    // each sync must have it's own shared memory
    m_shmName = "bshm" + std::to_string(globalContext);

    // same name mutex must be used in syncd
    boost::interprocess::named_mutex mtx(boost::interprocess::open_or_create_t(), BOOST_SHM_MUTEX_GLOBAL_NAME);

    bool owner = false;

    try
    {
        m_smo = std::make_shared<boost::interprocess::shared_memory_object>(
                boost::interprocess::create_only,
                m_shmName.c_str(),
                boost::interprocess::read_write);

        owner = true;

        m_smo->truncate(sizeof(boost_shm_data) + BOOST_SHM_RESPONSE_BUFFER_SIZE);

        SWSS_LOG_NOTICE("i'm shm owner\n");
    }
    catch(boost::interprocess::interprocess_exception &ex)
    {
        m_smo = std::make_shared<boost::interprocess::shared_memory_object>(
                boost::interprocess::open_only,
                m_shmName.c_str(),
                boost::interprocess::read_write);

        SWSS_LOG_NOTICE("i'm NOT shm owner\n");
    }

    m_mr = std::make_shared<boost::interprocess::mapped_region>(*m_smo, boost::interprocess::read_write);

    void* addr = m_mr->get_address();

    if (owner)
    {
        m_data = new (addr) boost_shm_data();

    }
    else
    {
        m_data = static_cast<boost_shm_data*>(addr);

        // TODO if both will set this to true, server will need to have it's own
        // then fail!, or allow only server to create, and client to open_only with timeout
        // m_data->m_clientNotOwner = true;
    }

    // start thread

    m_runNotificationThread = true;

    SWSS_LOG_NOTICE("creating notification thread");

    m_notificationThread = std::make_shared<std::thread>(&BoostSHMChannel::notificationThreadFunction, this);
}

BoostSHMChannel::~BoostSHMChannel()
{
    SWSS_LOG_ENTER();

    m_runNotificationThread = false;

    SWSS_LOG_NOTICE("join ntf thread begin");

    m_notificationThread->join();

    SWSS_LOG_NOTICE("join ntf thread end");

    // TODO those should be executed on exit/exception

    boost::interprocess::shared_memory_object::remove(m_shmName.c_str());

    boost::interprocess::named_mutex::remove(BOOST_SHM_MUTEX_GLOBAL_NAME);
}

void BoostSHMChannel::notificationThreadFunction()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("start listening for notifications");

    std::vector<uint8_t> buffer;

    buffer.resize(BOOST_SHM_RESPONSE_BUFFER_SIZE);

    while (m_runNotificationThread)
    {
        SWSS_LOG_WARN("FIXME");

        break;

        // buffer.at(rc) = 0; // make sure that we end string with zero before parse

        SWSS_LOG_DEBUG("ntf: %s", buffer.data());

        std::vector<swss::FieldValueTuple> values;

        swss::JSon::readJson((char*)buffer.data(), values);

        swss::FieldValueTuple fvt = values.at(0);

        const std::string& op = fvField(fvt);
        const std::string& data = fvValue(fvt);

        values.erase(values.begin());

        SWSS_LOG_DEBUG("notification: op = %s, data = %s", op.c_str(), data.c_str());

        m_callback(op, data, values);
    }

    SWSS_LOG_NOTICE("exiting notification thread");
}

void BoostSHMChannel::setBuffered(
        _In_ bool buffered)
{
    SWSS_LOG_ENTER();

    // not supported
}

void BoostSHMChannel::flush()
{
    SWSS_LOG_ENTER();

    // not supported
}

void BoostSHMChannel::set(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& values,
        _In_ const std::string& command)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> copy = values;

    swss::FieldValueTuple opdata(key, command);

    copy.insert(copy.begin(), opdata);

    std::string msg = swss::JSon::buildJson(copy);

    SWSS_LOG_DEBUG("sending: %s", msg.c_str());

    if (msg.length() >= BOOST_SHM_RESPONSE_BUFFER_SIZE)
    {
        SWSS_LOG_THROW("message too long: %zu > %zu", msg.length(), BOOST_SHM_RESPONSE_BUFFER_SIZE);
    }

    memcpy(m_data->m_buffer, msg.c_str(), msg.length());

    m_data->m_buffer[msg.length()] = 0;

    {
        std::unique_lock<boost::interprocess::interprocess_mutex> lck(m_data->m_mutex);

        m_data->m_dataReadyOut = true;
    }

    m_data->m_cond_out.notify_all(); // TODO should this be under mutex?
}

void BoostSHMChannel::del(
        _In_ const std::string& key,
        _In_ const std::string& command)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    swss::FieldValueTuple opdata(key, command);

    values.insert(values.begin(), opdata);

    std::string msg = swss::JSon::buildJson(values);

    SWSS_LOG_DEBUG("sending: %s", msg.c_str());

    if (msg.length() >= BOOST_SHM_RESPONSE_BUFFER_SIZE)
    {
        SWSS_LOG_THROW("message too long: %zu > %zu", msg.length(), BOOST_SHM_RESPONSE_BUFFER_SIZE);
    }

    memcpy(m_data->m_buffer, msg.c_str(), msg.length());

    m_data->m_buffer[msg.length()] = 0;

    {
        std::unique_lock<boost::interprocess::interprocess_mutex> lck(m_data->m_mutex);

        m_data->m_dataReadyOut = true;
    }

    m_data->m_cond_out.notify_all(); // TODO should be under mutex?
}

sai_status_t BoostSHMChannel::wait(
        _In_ const std::string& command,
        _Out_ swss::KeyOpFieldsValuesTuple& kco)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("wait for %s response", command.c_str());

    {
        std::unique_lock<boost::interprocess::interprocess_mutex> lck(m_data->m_mutex);

        m_data->m_cond_in.wait(lck, [&]{ return m_data->m_dataReadyIn; }); // TODO need timed wait

        // TODO check if data is available or timeout

        m_data->m_dataReadyIn = false;
    }

    SWSS_LOG_DEBUG("response: %s", m_data->m_buffer);

    std::vector<swss::FieldValueTuple> values;

    swss::JSon::readJson((char*)m_data->m_buffer, values);

    swss::FieldValueTuple fvt = values.at(0);

    const std::string& opkey = fvField(fvt);
    const std::string& op= fvValue(fvt);

    values.erase(values.begin());

    kfvFieldsValues(kco) = values;
    kfvOp(kco) = op;
    kfvKey(kco) = opkey;

    SWSS_LOG_INFO("response: op = %s, key = %s", opkey.c_str(), op.c_str());

    if (op != command)
    {
        // we can hit this place if there were some timeouts
        // as well, if there will be multiple "GET" messages, then
        // we can receive response from not the expected GET

        SWSS_LOG_THROW("got not expected response: %s:%s, expected: %s", opkey.c_str(), op.c_str(), command.c_str());
    }

    sai_status_t status;
    sai_deserialize_status(opkey, status);

    SWSS_LOG_DEBUG("%s status: %s", command.c_str(), opkey.c_str());

    return status;
}
