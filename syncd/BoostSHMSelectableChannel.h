#pragma once

#include "SelectableChannel.h"

#include "swss/table.h"
#include "swss/selectableevent.h"

#include "lib/inc/BoostSHMChannel.h"

#include <deque>
#include <thread>
#include <memory>

namespace syncd
{
    class BoostSHMSelectableChannel:
        public SelectableChannel
    {
        public:

            BoostSHMSelectableChannel(
                    _In_ uint32_t globalContext);

            virtual ~BoostSHMSelectableChannel();

        public: // SelectableChannel overrides

            virtual bool empty() override;

            virtual void pop(
                    _Out_ swss::KeyOpFieldsValuesTuple& kco,
                    _In_ bool initViewMode) override;

            virtual void set(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& values,
                    _In_ const std::string& op) override;

        public: // Selectable overrides

            virtual int getFd() override;

            virtual uint64_t readData() override;

            virtual bool hasData() override;

            virtual bool hasCachedData() override;

            // virtual bool initializedWithData() override;

            // virtual void updateAfterRead() override;

            // virtual int getPri() const override;

        private:

            void boostSHMThread();

        private:

            uint32_t m_globalContext;

            std::string m_shmName;

            std::shared_ptr<boost::interprocess::shared_memory_object> m_smo;

            std::shared_ptr<boost::interprocess::mapped_region> m_mr;

            sairedis::boost_shm_data* m_data;

            std::string m_buf;

            std::queue<std::string> m_queue;

            volatile bool m_runThread;

            std::shared_ptr<std::thread> m_boostSHMThread;

            swss::SelectableEvent m_selectableEvent;
    };
}
