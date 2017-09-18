#include "sai_vs.h"
#include "sai_vs_internal.h"

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

// TODO we can make this as a macro, and use only name of acual internal method
// then we will know that all metadata is ok for attributes

// TODO should this functionality be controlled from config.ini?
// since when doing reply only with syncd we don't need actual interfaces
// hostif to be created

sai_status_t vs_create_hostif_int(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t *hostif_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO validate input params, colistions, dulicates etc

    // SAI_OBJECT_TYPE_HOSTIF:oid:0xd00000000058d
    //
    // SAI_HOSTIF_ATTR_TYPE=SAI_HOSTIF_TYPE_NETDEV
    // SAI_HOSTIF_ATTR_OBJ_ID=oid:0x1000000000002
    // SAI_HOSTIF_ATTR_NAME=Ethernet0

    auto attr_name = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_NAME, attr_count, attr_list);

    if (attr_name == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_NAME was not passed");

        return SAI_STATUS_FAILURE;
    }

    std::string name = std::string(attr_name->value.chardata);

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

    // TODO what about FDB entries notifications, they also should
    // be generated if new mac addres will show up on the interface/arp table

    SWSS_LOG_INFO("created tap interface %d", name.c_str());

    // TODO create router interface should update MAC address on that hostif since all
    // should have mac of the switch

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
