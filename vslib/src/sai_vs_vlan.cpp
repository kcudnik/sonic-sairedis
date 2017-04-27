#include "sai_vs.h"
#include "sai_vs_state.h"
#include <map>
#include <set>
#include <vector>

sai_status_t vs_create_vlan(
        _Out_ sai_object_id_t *vlan_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    auto &vlan_members_map = g_switch_state_map.at(switch_id)->vlan_members_map;

    sai_status_t status = meta_sai_create_oid(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id,
            switch_id,
            attr_count,
            attr_list,
            &vs_generic_create);

    if (status == SAI_STATUS_SUCCESS)
    {
        vlan_members_map[*vlan_id] = {};
    }

    return status;
}

sai_status_t vs_remove_vlan(
        _In_ sai_object_id_t vlan_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_remove_oid(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id,
            &vs_generic_remove);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto &vlan_members_map = g_switch_state_map.at(sai_switch_id_query(vlan_id))->vlan_members_map;

        auto it = vlan_members_map.find(vlan_id);

        if (it == vlan_members_map.end())
        {
            SWSS_LOG_ERROR("failed to find vlan %s in vlan members map", 
                    sai_serialize_object_id(vlan_id).c_str());

            return SAI_STATUS_FAILURE;
        }

        vlan_members_map.erase(it);
    }

    return status;
}

sai_status_t vs_set_vlan_attribute(
        _In_ sai_object_id_t vlan_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_vlan_attribute(
        _In_ sai_object_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

void update_vlan_member_list_on_vlan(
        _In_ sai_object_id_t vlan_id)
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> vlan_member_list;

    sai_attribute_t attr;

    auto &vlan_members_map = g_switch_state_map.at(sai_switch_id_query(vlan_id))->vlan_members_map;

    std::copy(vlan_members_map[vlan_id].begin(), vlan_members_map[vlan_id].end(), std::back_inserter(vlan_member_list));

    uint32_t members_count = (uint32_t)vlan_member_list.size();

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;
    attr.value.objlist.count = members_count;
    attr.value.objlist.list = vlan_member_list.data();

    sai_status_t status = vs_generic_set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to update vlan %s members list: %s", 
                sai_serialize_object_id(vlan_id).c_str(), 
                sai_serialize_status(status).c_str());
    }

    SWSS_LOG_INFO("updated vlan %s member list to %zu", 
            sai_serialize_object_id(vlan_id).c_str(),
            vlan_member_list.size());
}

sai_object_id_t get_vlan_id_from_member(
        _In_ sai_object_id_t vlan_member_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            1,
            &attr);

    if (status == SAI_STATUS_SUCCESS)
    {
        return attr.value.oid;
    }

    SWSS_LOG_ERROR("failed to get vlan id from vlan member 0x%lx, status: %d", vlan_member_id, status);

    throw std::runtime_error("failed to get vlan id from vlan member");
}

sai_status_t vs_create_vlan_member(
        _Out_ sai_object_id_t* vlan_member_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_create_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            switch_id,
            attr_count,
            attr_list,
            &vs_generic_create);

    if (status == SAI_STATUS_SUCCESS)
    {
        sai_object_id_t vlan_id = get_vlan_id_from_member(*vlan_member_id);

        auto &vlan_members_map = g_switch_state_map.at(sai_switch_id_query(vlan_id))->vlan_members_map;

        vlan_members_map[vlan_id].insert(*vlan_member_id);

        update_vlan_member_list_on_vlan(vlan_id);
    }

    return status;
}

sai_status_t vs_remove_vlan_member(
        _In_ sai_object_id_t vlan_member_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    sai_object_id_t vlan_id = get_vlan_id_from_member(vlan_member_id);

    sai_status_t status = meta_sai_remove_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            &vs_generic_remove);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto &vlan_members_map = g_switch_state_map.at(sai_switch_id_query(vlan_id))->vlan_members_map;

        auto it = vlan_members_map[vlan_id].find(vlan_member_id);

        if (it == vlan_members_map[vlan_id].end())
        {
            SWSS_LOG_ERROR("failed to find vlan member 0x%lx in vlan members map", vlan_member_id);

            return SAI_STATUS_FAILURE;
        }

        vlan_members_map[vlan_id].erase(it);

        update_vlan_member_list_on_vlan(vlan_id);
    }

    return status;
}

sai_status_t vs_set_vlan_member_attribute(
        _In_ sai_object_id_t vlan_member_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_vlan_member_attribute(
        _In_ sai_object_id_t vlan_member_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_create_vlan_members(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attrs,
        _In_ sai_bulk_op_type_t type,
        _Out_ sai_object_id_t *object_id,
        _Out_ sai_status_t *object_statuses)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_remove_vlan_members(
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_type_t type,
        _Out_ sai_status_t *object_statuses)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_get_vlan_stats(
        _In_ sai_object_id_t vlan_id,
        _In_ const sai_vlan_stat_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t *counters)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_vlan_stats(
        _In_ sai_object_id_t vlan_id,
        _In_ const sai_vlan_stat_t *counter_ids,
        _In_ uint32_t number_of_counters)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_vlan_api_t vs_vlan_api = {
    vs_create_vlan,
    vs_remove_vlan,
    vs_set_vlan_attribute,
    vs_get_vlan_attribute,
    vs_create_vlan_member,
    vs_remove_vlan_member,
    vs_set_vlan_member_attribute,
    vs_get_vlan_member_attribute,
    vs_create_vlan_members,
    vs_remove_vlan_members,
    vs_get_vlan_stats,
    vs_clear_vlan_stats,
};
