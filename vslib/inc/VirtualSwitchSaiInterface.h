#pragma once

#include "lib/inc/SaiInterface.h"

#include <string>
#include <vector>

#define SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_REMOVE_ENTRY(ot)    \
    virtual sai_status_t remove(                                    \
            _In_ const sai_ ## ot ## _t* ot) override;

#define SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_CREATE_ENTRY(ot)    \
    virtual sai_status_t create(                                    \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _In_ const sai_attribute_t *attr_list) override;

#define SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_SET_ENTRY(ot)       \
    virtual sai_status_t set(                                       \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ const sai_attribute_t *attr) override;

#define SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_GET_ENTRY(ot)       \
    virtual sai_status_t get(                                       \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _Out_ sai_attribute_t *attr_list) override;

#define SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_CREATE_ENTRY(ot)   \
    virtual sai_status_t bulkCreate(                                    \
            _In_ uint32_t object_count,                                 \
            _In_ const sai_ ## ot ## _t *ot,                            \
            _In_ const uint32_t *attr_count,                            \
            _In_ const sai_attribute_t **attr_list,                     \
            _In_ sai_bulk_op_error_mode_t mode,                         \
            _Out_ sai_status_t *object_statuses) override;

#define SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_REMOVE_ENTRY(ot)   \
    virtual sai_status_t bulkRemove(                                    \
            _In_ uint32_t object_count,                                 \
            _In_ const sai_ ## ot ## _t *ot,                            \
            _In_ sai_bulk_op_error_mode_t mode,                         \
            _Out_ sai_status_t *object_statuses) override;

#define SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_SET_ENTRY(ot)      \
    virtual sai_status_t bulkSet(                                       \
            _In_ uint32_t object_count,                                 \
            _In_ const sai_ ## ot ## _t *ot,                            \
            _In_ const sai_attribute_t *attr_list,                      \
            _In_ sai_bulk_op_error_mode_t mode,                         \
            _Out_ sai_status_t *object_statuses) override;

namespace saivs
{
    class VirtualSwitchSaiInterface:
        public sairedis::SaiInterface
    {
        public:

            VirtualSwitchSaiInterface();

            virtual ~VirtualSwitchSaiInterface();

        public: // SAI interface overrides

            virtual sai_status_t create(
                    _In_ sai_object_type_t objectType,
                    _Out_ sai_object_id_t* objectId,
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

            virtual sai_status_t remove(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId) override;

            virtual sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr) override;

            virtual sai_status_t get(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list) override;

        public: // create ENTRY

            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_CREATE_ENTRY(fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_CREATE_ENTRY(inseg_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_CREATE_ENTRY(ipmc_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_CREATE_ENTRY(l2mc_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_CREATE_ENTRY(mcast_fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_CREATE_ENTRY(neighbor_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_CREATE_ENTRY(route_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_CREATE_ENTRY(nat_entry);

        public: // remove ENTRY

            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_REMOVE_ENTRY(fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_REMOVE_ENTRY(inseg_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_REMOVE_ENTRY(ipmc_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_REMOVE_ENTRY(l2mc_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_REMOVE_ENTRY(mcast_fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_REMOVE_ENTRY(neighbor_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_REMOVE_ENTRY(route_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_REMOVE_ENTRY(nat_entry);

        public: // set ENTRY

            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_SET_ENTRY(fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_SET_ENTRY(inseg_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_SET_ENTRY(ipmc_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_SET_ENTRY(l2mc_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_SET_ENTRY(mcast_fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_SET_ENTRY(neighbor_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_SET_ENTRY(route_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_SET_ENTRY(nat_entry);

        public: // get ENTRY

            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_GET_ENTRY(fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_GET_ENTRY(inseg_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_GET_ENTRY(ipmc_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_GET_ENTRY(l2mc_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_GET_ENTRY(mcast_fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_GET_ENTRY(neighbor_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_GET_ENTRY(route_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_GET_ENTRY(nat_entry);

        public: // bulk QUAD oid

            virtual sai_status_t bulkCreate(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t object_count,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_object_id_t *object_id,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkRemove(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkSet(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

        public: // bulk create ENTRY

            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_CREATE_ENTRY(fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_CREATE_ENTRY(nat_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_CREATE_ENTRY(route_entry);

        public: // bulk remove ENTRY

            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_REMOVE_ENTRY(fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_REMOVE_ENTRY(nat_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_REMOVE_ENTRY(route_entry);

        public: // bulk set ENTRY

            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_SET_ENTRY(fdb_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_SET_ENTRY(nat_entry);
            SAIVS_VIRTUALSWITCHSAIINTERFACE_DECLARE_BULK_SET_ENTRY(route_entry);

        public: // stats API

            virtual sai_status_t getStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t getStatsExt(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t clearStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids) override;

        public: // non QUAD API

            virtual sai_status_t flushFdbEntries(
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList) override;

        public: // SAI API

            virtual sai_status_t objectTypeGetAvailability(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList,
                    _Out_ uint64_t *count) override;

            virtual sai_status_t queryAattributeEnumValuesCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Inout_ sai_s32_list_t *enum_values_capability) override;

            virtual sai_object_type_t objectTypeQuery(
                    _In_ sai_object_id_t objectId) override;

            virtual sai_object_id_t switchIdQuery(
                    _In_ sai_object_id_t objectId) override;

        private: // QUAD API helpers

            sai_status_t create(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t remove(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId);

            sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId,
                    _In_ const sai_attribute_t *attr);

            sai_status_t get(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

        private: // bulk QUAD API helpers

            sai_status_t bulkCreate(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Inout_ sai_status_t *object_statuses);

            sai_status_t bulkRemove(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

            sai_status_t bulkSet(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

        private: // QUAD pre

            sai_status_t preSet(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr);

            sai_status_t preSetPort(
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr);

        private:

    };
}
