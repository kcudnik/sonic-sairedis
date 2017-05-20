#include "sai_vs.h"
#include "sai_vs_state.h"

sai_status_t refresh_read_only(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id)
{
    // TODO read only, must be per switch since maybe different
    // implemented, different order maybe on the queues


    // SAI_BRIDGE_ATTR_PORT_LIST
    // SAI_BRIDGE_PORT_ATTR_PORT_ID
    // SAI_BRIDGE_PORT_ATTR_TYPE
    // SAI_PORT_ATTR_HW_LANE_LIST
    // SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST
    // SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS
    // SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES
    // SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS
    // SAI_PORT_ATTR_QOS_QUEUE_LIST
    // SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST
    // SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT
    // SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST
    // SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY
    // SAI_SWITCH_ATTR_CPU_PORT
    // SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP
    // SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID
    // SAI_SWITCH_ATTR_DEFAULT_VLAN_ID
    // SAI_SWITCH_ATTR_PORT_LIST
    // SAI_SWITCH_ATTR_PORT_NUMBER
    // SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP
    // SAI_SWITCH_ATTR_SRC_MAC_ADDRESS
    // SAI_VLAN_ATTR_MEMBER_LIST
    // SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID

    SWSS_LOG_WARN("need to recalculate RO: %s", meta->attridname);

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t internal_vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto &objectHash = g_switch_state_map.at(switch_id)->objectHash;

    auto it = objectHash.find(serialized_object_id);

    if (it == objectHash.end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(object_type).c_str(),
                serialized_object_id.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    AttrHash attrHash = it->second;

    /*
     * Some of the list query maybe for length, so we can't do
     * normal serialize, maybe with count only.
     */

    sai_status_t final_status = SAI_STATUS_SUCCESS;

    for (uint32_t idx = 0; idx < attr_count; ++idx)
    {
        sai_attr_id_t id = attr_list[idx].id;

        auto meta = sai_metadata_get_attr_metadata(object_type, id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("failed to find attribute %d for %s:%s", id,
                    sai_serialize_object_type(object_type).c_str(),
                    serialized_object_id.c_str());

            return SAI_STATUS_FAILURE;
        }

        sai_status_t status;

        if (HAS_FLAG_READ_ONLY(meta->flags))
        {
            /*
             * Read only attributes may require recalculation.
             * Metadata makes sure that non object id's can't have
             * read only attributes. So here is defenetly OID.
             */

            sai_object_id_t oid;
            sai_deserialize_object_id(serialized_object_id, oid);

            status = refresh_read_only(meta, oid, switch_id);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("%s not implemented on %s",
                        meta->attridname,
                        serialized_object_id.c_str());

                return status;
            }
        }

        auto ait = attrHash.find(meta->attridname);

        if (ait == attrHash.end())
        {
            SWSS_LOG_ERROR("%s not implemented on %s",
                    meta->attridname,
                    serialized_object_id.c_str());

            return SAI_STATUS_NOT_IMPLEMENTED;
        }

        auto attr = ait->second->getAttr();

        status = transfer_attributes(object_type, 1, attr, &attr_list[idx], false);

        if (status == SAI_STATUS_BUFFER_OVERFLOW)
        {
            /*
             * This is considered partial success, since we get correct list
             * length.  Note that other items ARE processes on the list.
             */

            SWSS_LOG_NOTICE("BUFFER_OVERFLOW %s: %s",
                    serialized_object_id.c_str(),
                    meta->attridname);

            /*
             * We still continue processing other attributes for get as long as
             * we only will be getting buffer overflow error.
             */

            final_status = status;
            continue;
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            // all other errors

            SWSS_LOG_ERROR("get failed %s: %s: %s",
                    serialized_object_id.c_str(),
                    meta->attridname,
                    sai_serialize_status(status).c_str());

            return status;
        }
    }

    return final_status;
}

sai_status_t vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    sai_object_id_t switch_id = sai_switch_id_query(object_id);

    return internal_vs_generic_get(
            object_type,
            str_object_id,
            switch_id,
            attr_count,
            attr_list);
}

sai_status_t vs_generic_get_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry = sai_serialize_fdb_entry(*fdb_entry);

    return internal_vs_generic_get(
            SAI_OBJECT_TYPE_FDB_ENTRY,
            str_fdb_entry,
            fdb_entry->switch_id,
            attr_count,
            attr_list);
}

sai_status_t vs_generic_get_neighbor_entry(
        _In_ const sai_neighbor_entry_t *neighbor_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_neighbor_entry = sai_serialize_neighbor_entry(*neighbor_entry);

    return internal_vs_generic_get(
            SAI_OBJECT_TYPE_NEIGHBOR_ENTRY,
            str_neighbor_entry,
            neighbor_entry->switch_id,
            attr_count,
            attr_list);
}

sai_status_t vs_generic_get_route_entry(
        _In_ const sai_route_entry_t *route_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_route_entry = sai_serialize_route_entry(*route_entry);

    return internal_vs_generic_get(
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            str_route_entry,
            route_entry->switch_id,
            attr_count,
            attr_list);
}

/*
SAI_BRIDGE_ATTR_PORT_LIST
SAI_BUFFER_POOL_ATTR_SHARED_SIZE
SAI_HOSTIF_PACKET_ATTR_HOSTIF_TRAP_ID
SAI_HOSTIF_PACKET_ATTR_INGRESS_PORT
SAI_HOSTIF_PACKET_ATTR_INGRESS_LAG
SAI_IPMC_GROUP_ATTR_IPMC_OUTPUT_COUNT
SAI_IPMC_GROUP_ATTR_IPMC_MEMBER_LIST
SAI_L2MC_GROUP_ATTR_L2MC_OUTPUT_COUNT
SAI_L2MC_GROUP_ATTR_L2MC_MEMBER_LIST
SAI_LAG_ATTR_PORT_LIST
SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_COUNT
SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_MEMBER_LIST
SAI_PORT_ATTR_TYPE
SAI_PORT_ATTR_OPER_STATUS
SAI_PORT_ATTR_SUPPORTED_BREAKOUT_MODE_TYPE
SAI_PORT_ATTR_CURRENT_BREAKOUT_MODE_TYPE
SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES
SAI_PORT_ATTR_QOS_QUEUE_LIST
SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS
SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST
SAI_PORT_ATTR_SUPPORTED_SPEED
SAI_PORT_ATTR_SUPPORTED_FEC_MODE
SAI_PORT_ATTR_SUPPORTED_HALF_DUPLEX_SPEED
SAI_PORT_ATTR_SUPPORTED_AUTO_NEG_MODE
SAI_PORT_ATTR_SUPPORTED_FLOW_CONTROL_MODE
SAI_PORT_ATTR_SUPPORTED_ASYMMETRIC_PAUSE_MODE
SAI_PORT_ATTR_SUPPORTED_MEDIA_TYPE
SAI_PORT_ATTR_REMOTE_ADVERTISED_SPEED
SAI_PORT_ATTR_REMOTE_ADVERTISED_FEC_MODE
SAI_PORT_ATTR_REMOTE_ADVERTISED_HALF_DUPLEX_SPEED
SAI_PORT_ATTR_REMOTE_ADVERTISED_AUTO_NEG_MODE
SAI_PORT_ATTR_REMOTE_ADVERTISED_FLOW_CONTROL_MODE
SAI_PORT_ATTR_REMOTE_ADVERTISED_ASYMMETRIC_PAUSE_MODE
SAI_PORT_ATTR_REMOTE_ADVERTISED_MEDIA_TYPE
SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS
SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST
SAI_QUEUE_ATTR_PAUSE_STATUS
SAI_RPF_GROUP_ATTR_RPF_INTERFACE_COUNT
SAI_RPF_GROUP_ATTR_RPF_MEMBER_LIST
SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT
SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST
SAI_STP_ATTR_VLAN_LIST
SAI_STP_ATTR_BRIDGE_ID
SAI_STP_ATTR_PORT_LIST
SAI_SWITCH_ATTR_PORT_NUMBER
SAI_SWITCH_ATTR_PORT_LIST
SAI_SWITCH_ATTR_PORT_MAX_MTU
SAI_SWITCH_ATTR_CPU_PORT
SAI_SWITCH_ATTR_MAX_VIRTUAL_ROUTERS
SAI_SWITCH_ATTR_FDB_TABLE_SIZE
SAI_SWITCH_ATTR_L3_NEIGHBOR_TABLE_SIZE
SAI_SWITCH_ATTR_L3_ROUTE_TABLE_SIZE
SAI_SWITCH_ATTR_LAG_MEMBERS
SAI_SWITCH_ATTR_NUMBER_OF_LAGS
SAI_SWITCH_ATTR_ECMP_MEMBERS
SAI_SWITCH_ATTR_NUMBER_OF_ECMP_GROUPS
SAI_SWITCH_ATTR_NUMBER_OF_UNICAST_QUEUES
SAI_SWITCH_ATTR_NUMBER_OF_MULTICAST_QUEUES
SAI_SWITCH_ATTR_NUMBER_OF_QUEUES
SAI_SWITCH_ATTR_NUMBER_OF_CPU_QUEUES
SAI_SWITCH_ATTR_ON_LINK_ROUTE_SUPPORTED
SAI_SWITCH_ATTR_OPER_STATUS
SAI_SWITCH_ATTR_MAX_TEMP
SAI_SWITCH_ATTR_ACL_TABLE_MINIMUM_PRIORITY
SAI_SWITCH_ATTR_ACL_TABLE_MAXIMUM_PRIORITY
SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY
SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY
SAI_SWITCH_ATTR_ACL_TABLE_GROUP_MINIMUM_PRIORITY
SAI_SWITCH_ATTR_ACL_TABLE_GROUP_MAXIMUM_PRIORITY
SAI_SWITCH_ATTR_FDB_DST_USER_META_DATA_RANGE
SAI_SWITCH_ATTR_ROUTE_DST_USER_META_DATA_RANGE
SAI_SWITCH_ATTR_NEIGHBOR_DST_USER_META_DATA_RANGE
SAI_SWITCH_ATTR_PORT_USER_META_DATA_RANGE
SAI_SWITCH_ATTR_VLAN_USER_META_DATA_RANGE
SAI_SWITCH_ATTR_ACL_USER_META_DATA_RANGE
SAI_SWITCH_ATTR_ACL_USER_TRAP_ID_RANGE
SAI_SWITCH_ATTR_DEFAULT_VLAN_ID
SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID
SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID
SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID
SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_PORT_ID
SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_TRAFFIC_CLASSES
SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_SCHEDULER_GROUP_HIERARCHY_LEVELS
SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_SCHEDULER_GROUPS_PER_HIERARCHY_LEVEL
SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP
SAI_SWITCH_ATTR_TOTAL_BUFFER_SIZE
SAI_SWITCH_ATTR_INGRESS_BUFFER_POOL_NUM
SAI_SWITCH_ATTR_EGRESS_BUFFER_POOL_NUM
SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP
SAI_SWITCH_ATTR_ECMP_HASH
SAI_SWITCH_ATTR_LAG_HASH
SAI_SWITCH_ATTR_RESTART_TYPE
SAI_SWITCH_ATTR_MIN_PLANNED_RESTART_INTERVAL
SAI_SWITCH_ATTR_NV_STORAGE_SIZE
SAI_SWITCH_ATTR_MAX_ACL_ACTION_COUNT
SAI_SWITCH_ATTR_MCAST_SNOOPING_CAPABILITY
SAI_SWITCH_ATTR_ECMP_HASH_IPV4
SAI_SWITCH_ATTR_ECMP_HASH_IPV4_IN_IPV4
SAI_SWITCH_ATTR_ECMP_HASH_IPV6
SAI_SWITCH_ATTR_LAG_HASH_IPV4
SAI_SWITCH_ATTR_LAG_HASH_IPV4_IN_IPV4
SAI_SWITCH_ATTR_LAG_HASH_IPV6
SAI_SWITCH_ATTR_ACL_STAGE_INGRESS
SAI_SWITCH_ATTR_ACL_STAGE_EGRESS
SAI_UDF_GROUP_ATTR_UDF_LIST
SAI_VLAN_ATTR_MEMBER_LIST
*/
