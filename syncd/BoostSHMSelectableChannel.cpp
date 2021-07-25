#include "BoostSHMSelectableChannel.h"

#include "swss/logger.h"
#include "swss/json.h"

#include <zmq.h>
#include <unistd.h>

#include <boost/interprocess/sync/named_mutex.hpp>

#define BOOST_SHM_GETRESPONSE_TIMEOUT_MS (2*60*1000)

using namespace syncd;
using namespace sairedis;

BoostSHMSelectableChannel::BoostSHMSelectableChannel(
        _In_ uint32_t globalContext):
    m_globalContext(globalContext),
    m_data(nullptr)
{
    SWSS_LOG_ENTER();

    // each sync must have it's own shared memory
    m_shmName = "bshm" + std::to_string(globalContext);

    // TODO remove, this is for testing only
    // TODO it may happen, that shm will stay in memory
    boost::interprocess::shared_memory_object::remove(m_shmName.c_str());
    boost::interprocess::named_mutex::remove(BOOST_SHM_MUTEX_GLOBAL_NAME);

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
    }

    m_runThread = true;

    m_boostSHMThread = std::make_shared<std::thread>(&BoostSHMSelectableChannel::boostSHMThread, this);
}

BoostSHMSelectableChannel::~BoostSHMSelectableChannel()
{
    SWSS_LOG_ENTER();

    m_data->m_cond_out.notify_all();

    m_runThread = false;

    SWSS_LOG_NOTICE("ending zmq poll thread");

    m_boostSHMThread = nullptr;

    SWSS_LOG_NOTICE("ended zmq poll thread");

    // TODO missing selectable ?

    // TODO those should be executed on exit/exception

    boost::interprocess::shared_memory_object::remove(m_shmName.c_str());

    boost::interprocess::named_mutex::remove(BOOST_SHM_MUTEX_GLOBAL_NAME);
}

void BoostSHMSelectableChannel::boostSHMThread()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    while (m_runThread)
    {
        std::unique_lock<boost::interprocess::interprocess_mutex> lck(m_data->m_mutex);

        m_data->m_cond_out.wait(lck, [&]{ return m_data->m_dataReadyOut; }); // TODO need timed wait

        if (!m_runThread)
            break;

        // TODO check if data is available or timeout

        m_data->m_dataReadyOut = false;

        m_buf = std::string((char*)m_data->m_buffer);

        m_selectableEvent.notify(); // will release epoll
    }

    SWSS_LOG_NOTICE("end");
}

// SelectableChannel overrides

bool BoostSHMSelectableChannel::empty()
{
    SWSS_LOG_ENTER();

    return m_queue.size() == 0;
}

void BoostSHMSelectableChannel::pop(
        _Out_ swss::KeyOpFieldsValuesTuple& kco,
        _In_ bool initViewMode)
{
    SWSS_LOG_ENTER();

    if (m_queue.empty())
    {
        SWSS_LOG_ERROR("queue is empty, can't pop");
        throw std::runtime_error("queue is empty, can't pop");
    }

    std::string msg = m_queue.front();
    m_queue.pop();

    auto& values = kfvFieldsValues(kco);

    values.clear();

    swss::JSon::readJson(msg, values);

    swss::FieldValueTuple fvt = values.at(0);

    kfvKey(kco) = fvField(fvt);
    kfvOp(kco) = fvValue(fvt);

    values.erase(values.begin());
}

void BoostSHMSelectableChannel::set(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& values,
        _In_ const std::string& op)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> copy = values;

    swss::FieldValueTuple opdata(key, op);

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

        m_data->m_dataReadyIn = true;
    }

    m_data->m_cond_in.notify_all(); // TODO should this be under mutex?
}

// Selectable overrides

int BoostSHMSelectableChannel::getFd()
{
    SWSS_LOG_ENTER();

    return m_selectableEvent.getFd();
}

uint64_t BoostSHMSelectableChannel::readData()
{
    SWSS_LOG_ENTER();

    // clear selectable event so it could be triggered in next select()
    m_selectableEvent.readData();

    m_queue.push(m_buf);

    return 0;
}

bool BoostSHMSelectableChannel::hasData()
{
    SWSS_LOG_ENTER();

    return m_queue.size() > 0;
}

bool BoostSHMSelectableChannel::hasCachedData()
{
    SWSS_LOG_ENTER();

    return m_queue.size() > 1;
}
