#pragma once

#include "Channel.h"

#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/notificationconsumer.h"
#include "swss/selectableevent.h"

#include <memory>
#include <functional>

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#define BOOST_SHM_RESPONSE_BUFFER_SIZE (4*1024*1024)

#define BOOST_SHM_MUTEX_GLOBAL_NAME "bshmmtx"

namespace sairedis
{
    typedef struct _boost_shm_data
    {
        _boost_shm_data():
            m_dataReadyOut(false),
            m_dataReadyIn(false)
        {
            // empty
        }

        // Mutex to protect access to the queue
        boost::interprocess::interprocess_mutex m_mutex;

        boost::interprocess::interprocess_condition m_cond_out;

        boost::interprocess::interprocess_condition m_cond_in;

        bool m_dataReadyOut;

        bool m_dataReadyIn;

        uint8_t m_buffer[1];

    } boost_shm_data;

    class BoostSHMChannel:
        public Channel
    {
        public:

            BoostSHMChannel(
                    _In_ uint32_t globalContext,
                    _In_ Channel::Callback callback);

            virtual ~BoostSHMChannel();

        public:

            virtual void setBuffered(
                    _In_ bool buffered) override;

            virtual void flush() override;

            virtual void set(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& values,
                    _In_ const std::string& command) override;

            virtual void del(
                    _In_ const std::string& key,
                    _In_ const std::string& command) override;

            virtual sai_status_t wait(
                    _In_ const std::string& command,
                    _Out_ swss::KeyOpFieldsValuesTuple& kco) override;

        protected:

            virtual void notificationThreadFunction() override;

        private:

            uint32_t m_globalContext;

            std::string m_shmName;

            std::shared_ptr<boost::interprocess::shared_memory_object> m_smo;

            std::shared_ptr<boost::interprocess::mapped_region> m_mr;

            boost_shm_data* m_data;
    };
}
