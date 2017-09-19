#include "sai_vs.h"
#include "sai_vs_internal.h"

#include "meta/saiserialize.h"

#define MAX_INTERFACE_NAME_LEN 16

sai_status_t vs_recv_hostif_packet(
        _In_ sai_object_id_t hif_id,
        _Out_ void *buffer,
        _Inout_ sai_size_t *buffer_size,
        _Inout_ uint32_t *attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_send_hostif_packet(
        _In_ sai_object_id_t hif_id,
        _In_ void *buffer,
        _In_ sai_size_t buffer_size,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_create_hostif_int(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t *hostif_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (g_vs_hostif_use_tap_device == false)
    {
        return SAI_STATUS_SUCCESS;
    }

    // validate SAI_HOSTIF_ATTR_TYPE

    auto attr_type = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_TYPE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }

    if (attr_type->value.s32 != SAI_HOSTIF_TYPE_NETDEV)
    {
        SWSS_LOG_ERROR("only SAI_HOSTIF_TYPE_NETDEV is supported");

        return SAI_STATUS_FAILURE;
    }

    // validate SAI_HOSTIF_ATTR_OBJ_ID

    auto attr_obj_id = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_OBJ_ID, attr_count, attr_list);

    if (attr_obj_id == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_OBJ_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t obj_id = attr_obj_id->value.oid;

    sai_object_type_t ot = sai_object_type_query(obj_id);

    if (ot != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("SAI_HOSTIF_ATTR_OBJ_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(obj_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }

    // validate SAI_HOSTIF_ATTR_NAME

    auto attr_name = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_NAME, attr_count, attr_list);

    if (attr_name == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_NAME was not passed");

        return SAI_STATUS_FAILURE;
    }

    if (strnlen(attr_name->value.chardata, sizeof(attr_name->value.chardata)) >= MAX_INTERFACE_NAME_LEN)
    {
        SWSS_LOG_ERROR("interface name is too long: %.*s", MAX_INTERFACE_NAME_LEN, attr_name->value.chardata);

        return SAI_STATUS_FAILURE;
    }

    if (strncmp(attr_name->value.chardata, "Ethernet", 8) != 0)
    {
        SWSS_LOG_ERROR("interface name should start with EthernetX but is %s", attr_name->value.chardata);

        return SAI_STATUS_FAILURE;
    }

    std::string name = std::string(attr_name->value.chardata);

    // create TAP device

    SWSS_LOG_INFO("creating hostif %s", name.c_str());

    // TODO make this in programatic way since when used without sudo it will prompt
    // and hang

    // TODO validate "name" if it's in format EthernetX

    std::string cmd = "/sbin/ip tuntap add name " + name + " mode tap";

    // normally interfaces are in state "DOWN" after create, but we have
    // configuration in /etc/network/interfaces and they are configured to UP

    int ret = system(cmd.c_str());

    if (ret)
    {
        SWSS_LOG_ERROR("creating tap interface %s with command '%s' failed: %d",
                name.c_str(),
                cmd.c_str(),
                ret);

        return SAI_STATUS_FAILURE;
    }

    // TODO update mac address

    // TODO what about FDB entries notifications, they also should
    // be generated if new mac addres will show up on the interface/arp table

    SWSS_LOG_INFO("created tap interface %d", name.c_str());

    return vs_generic_create(object_type,
            hostif_id,
            switch_id,
            attr_count,
            attr_list);
}

sai_status_t vs_create_hostif(
        _Out_ sai_object_id_t *hostif_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_HOSTIF,
            hostif_id,
            switch_id,
            attr_count,
            attr_list,
            &vs_create_hostif_int);
}

// TODO set must also be supported when we change oper status up/down
// and probably also generate notification then

VS_REMOVE(HOSTIF,hostif);
VS_SET(HOSTIF,hostif);
VS_GET(HOSTIF,hostif);

VS_GENERIC_QUAD(HOSTIF_TABLE_ENTRY,hostif_table_entry);
VS_GENERIC_QUAD(HOSTIF_TRAP_GROUP,hostif_trap_group);
VS_GENERIC_QUAD(HOSTIF_TRAP,hostif_trap);
VS_GENERIC_QUAD(HOSTIF_USER_DEFINED_TRAP,hostif_user_defined_trap);

const sai_hostif_api_t vs_hostif_api = {

    VS_GENERIC_QUAD_API(hostif)
    VS_GENERIC_QUAD_API(hostif_table_entry)
    VS_GENERIC_QUAD_API(hostif_trap_group)
    VS_GENERIC_QUAD_API(hostif_trap)
    VS_GENERIC_QUAD_API(hostif_user_defined_trap)

    vs_recv_hostif_packet,
    vs_send_hostif_packet,
};
