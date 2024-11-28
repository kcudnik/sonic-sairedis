#pragma once

#include "SwitchStateBase.h"

namespace saivs
{
    class SwitchVpp:
        public SwitchStateBase
    {
        public:

            SwitchVpp(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config);

            SwitchVpp(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config,
                    _In_ std::shared_ptr<WarmBootState> warmBootState);

            virtual ~SwitchVpp() = default;

        private: // from vpp VirtualSwitchSaiInterface

            void setPortStats(
                    _In_ sai_object_id_t oid);

            bool port_to_hostif_list(
                    _In_ sai_object_id_t oid,
                    _Inout_ std::string& if_name);

            bool port_to_hwifname(
                    _In_ sai_object_id_t oid,
                    _Inout_ std::string& if_name);

        public: // from VirtualSwitchSaiInterface changed functions

            virtual sai_status_t queryAttributeCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Out_ sai_attr_capability_t *capability) override;

            virtual sai_status_t getStatsExt(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Out_ uint64_t *counters) override;

            virtual void processFdbEntriesForAging() override;

        private:

            // std::map<sai_object_id_t, std::string> phMap; // TODO to be removed
    };
}
