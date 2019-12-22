#include "sai_redis.h"
#include "sairedis.h"
#include "sairediscommon.h"

#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"

#include "swss/selectableevent.h"
#include <string.h>

#include "VirtualObjectIdManager.h"
#include "RedisVidIndexGenerator.h"
#include "RedisRemoteSaiInterface.h"
#include "WrapperRemoteSaiInterface.h"

using namespace sairedis;
using namespace saimeta;

sai_service_method_table_t g_services;

// TODO must be per syncd instance
std::shared_ptr<SwitchContainer>            g_switchContainer;
std::shared_ptr<Recorder>                   g_recorder;
std::shared_ptr<RemoteSaiInterface>         g_remoteSaiInterface;
std::shared_ptr<Meta>                       g_meta;

void clear_local_state()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("clearing local state");

    // Will need to be executed after init VIEW

    // will clear switch container
    g_switchContainer = std::make_shared<SwitchContainer>();

    // Initialize metadata database.
    // TODO must be done per syncd instance
    meta_init_db();

    // TODO since we create new manager, we need to create new meta db with
    // updated functions for query object type and switch id
    // TODO update global context when supporting multiple syncd instances
    g_virtualObjectIdManager = std::make_shared<VirtualObjectIdManager>(0, g_redisVidIndexGenerator);

    // TODO same as meta init database, will be the same at the end
    g_meta = std::make_shared<Meta>();
}

sai_status_t sai_api_initialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t* services)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (Globals::apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api already initialized", __PRETTY_FUNCTION__);

        return SAI_STATUS_FAILURE;
    }

    if ((NULL == services) || (NULL == services->profile_get_next_value) || (NULL == services->profile_get_value))
    {
        SWSS_LOG_ERROR("Invalid services handle passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&g_services, services, sizeof(g_services));

    g_recorder = std::make_shared<Recorder>();

    auto impl = std::make_shared<RedisRemoteSaiInterface>();

    g_remoteSaiInterface = std::make_shared<WrapperRemoteSaiInterface>(impl);

    clear_local_state();

    Globals::apiInitialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_api_uninitialize(void)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    g_remoteSaiInterface = nullptr;
    g_recorder = nullptr;

    Globals::apiInitialized = false;

    return SAI_STATUS_SUCCESS;
}

// FIXME: Currently per API log level cannot be set
sai_status_t sai_log_set(
        _In_ sai_api_t sai_api_id,
        _In_ sai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

#define API_CASE(API,api)\
    case SAI_API_ ## API: {\
        *(const sai_ ## api ## _api_t**)api_method_table = &redis_ ## api ## _api;\
        return SAI_STATUS_SUCCESS; }

sai_status_t sai_api_query(
        _In_ sai_api_t sai_api_id,
        _Out_ void** api_method_table)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    if (api_method_table == NULL)
    {
        SWSS_LOG_ERROR("NULL method table passed to SAI API initialize");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (sai_api_id)
    {
        API_CASE(ACL,acl);
        API_CASE(BFD,bfd);
        API_CASE(BMTOR,bmtor);
        API_CASE(BRIDGE,bridge);
        API_CASE(BUFFER,buffer);
        API_CASE(COUNTER,counter);
        API_CASE(DEBUG_COUNTER,debug_counter);
        API_CASE(DTEL,dtel);
        API_CASE(FDB,fdb);
        API_CASE(HASH,hash);
        API_CASE(HOSTIF,hostif);
        API_CASE(IPMC_GROUP,ipmc_group);
        API_CASE(IPMC,ipmc);
        API_CASE(ISOLATION_GROUP,isolation_group);
        API_CASE(L2MC_GROUP,l2mc_group);
        API_CASE(L2MC,l2mc);
        API_CASE(LAG,lag);
        API_CASE(MCAST_FDB,mcast_fdb);
        API_CASE(MIRROR,mirror);
        API_CASE(MPLS,mpls);
        API_CASE(NAT,nat);
        API_CASE(NEIGHBOR,neighbor);
        API_CASE(NEXT_HOP_GROUP,next_hop_group);
        API_CASE(NEXT_HOP,next_hop);
        API_CASE(POLICER,policer);
        API_CASE(PORT,port);
        API_CASE(QOS_MAP,qos_map);
        API_CASE(QUEUE,queue);
        API_CASE(ROUTER_INTERFACE,router_interface);
        API_CASE(ROUTE,route);
        API_CASE(RPF_GROUP,rpf_group);
        API_CASE(SAMPLEPACKET,samplepacket);
        API_CASE(SCHEDULER_GROUP,scheduler_group);
        API_CASE(SCHEDULER,scheduler);
        API_CASE(SEGMENTROUTE,segmentroute);
        API_CASE(STP,stp);
        API_CASE(SWITCH,switch);
        API_CASE(TAM,tam);
        API_CASE(TUNNEL,tunnel);
        API_CASE(UDF,udf);
        API_CASE(VIRTUAL_ROUTER,virtual_router);
        API_CASE(VLAN,vlan);
        API_CASE(WRED,wred);

        default:
            SWSS_LOG_ERROR("Invalid API type %d", sai_api_id);
            return SAI_STATUS_INVALID_PARAMETER;
    }
}

sai_status_t sai_query_attribute_enum_values_capability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return g_meta->queryAattributeEnumValuesCapability(
            switch_id,
            object_type,
            attr_id,
            enum_values_capability,
            *g_remoteSaiInterface);
}

sai_status_t sai_object_type_get_availability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _Out_ uint64_t *count)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return g_meta->objectTypeGetAvailability(
            switch_id,
            object_type,
            attr_count,
            attr_list,
            count,
            *g_remoteSaiInterface);
}

sai_object_type_t sai_object_type_query(
        _In_ sai_object_id_t objectId)
{
    // TODO must be protected using api MUTEX (but first remove usage from metadata) - use std::function

    SWSS_LOG_ENTER();

    if (!Globals::apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);

        return SAI_OBJECT_TYPE_NULL;
    }

    // TODO support global context

    return g_virtualObjectIdManager->saiObjectTypeQuery(objectId);
}

sai_object_id_t sai_switch_id_query(
        _In_ sai_object_id_t objectId)
{
    // TODO must be protected using api MUTEX (but first remove usage from metadata) - use std::function

    SWSS_LOG_ENTER();

    if (!Globals::apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);

        return SAI_NULL_OBJECT_ID;
    }

    // TODO support global context

    return g_virtualObjectIdManager->saiSwitchIdQuery(objectId);
}
