#include "sai_vs.h"
#include "sai_vs_state.h"

void refresh_read_only(
        _In_ const sai_attr_metadata_t *meta,
        _In_ const std::string &serialized_object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    // TODO!

    SWSS_LOG_WARN("need to recalculate RO: %s", meta->attridname);
}

sai_status_t internal_vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto & objectHash = g_switch_state_map.at(switch_id)->objectHash;

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

        if (HAS_FLAG_READ_ONLY(meta->flags))
        {
            // read only attributes may require recalculation
            refresh_read_only(meta, serialized_object_id, switch_id, attr_count, attr_list);
        }

        auto ait = attrHash.find(meta->attridname);

        if (ait == attrHash.end())
        {
            SWSS_LOG_ERROR("attribute %s not found on %s:%s",
                    meta->attridname,
                    sai_serialize_object_type(object_type).c_str(),
                    serialized_object_id.c_str());

            return SAI_STATUS_ITEM_NOT_FOUND;
        }

        auto attr = ait->second->getAttr();

        sai_status_t status = transfer_attributes(object_type, 1, attr, &attr_list[idx], false);

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
