#include "syncd.h"
#include "syncd_saiswitch.h"
#include "sairedis.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <set>

/*
 * NOTE: all those methods could be implemented inside SaiSwitch class so then
 * we could skip using switch_id in params and even they could be public then.
 */

sai_uint32_t SaiSwitch::saiGetPortCount()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    sai_status_t status = sai_metadata_sai_switch_api->get_switch_attribute(m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to get port number: %s",
                sai_serialize_status(status).c_str());
    }

    SWSS_LOG_DEBUG("port count is %u", attr.value.u32);

    return attr.value.u32;
}

#define MAX_HARDWARE_INFO_LENGTH 0x1000

std::string SaiSwitch::saiGetHardwareInfo()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    char info[MAX_HARDWARE_INFO_LENGTH];

    memset(info, 0, MAX_HARDWARE_INFO_LENGTH);

    attr.id = SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO;

    attr.value.s8list.count = MAX_HARDWARE_INFO_LENGTH;
    attr.value.s8list.list = (int8_t*)info;

    sai_status_t status = sai_metadata_sai_switch_api->get_switch_attribute(m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        /*
         * TODO: We should have THROW here, but currently getting hardware info
         * is not supported, so we just return empty string like it's not set.
         * Later on basing on this entry we will distinquish whether previous
         * switch and next switch are the same.
         */
        SWSS_LOG_ERROR("failed to get switch hardware info: %s",
                sai_serialize_status(status).c_str());
    }

    SWSS_LOG_DEBUG("hardware info: '%s'", info);

    m_hardware_info = std::string(info);

    return m_hardware_info;
}

std::vector<sai_object_id_t> SaiSwitch::saiGetPortList()
{
    SWSS_LOG_ENTER();

    uint32_t portCount = saiGetPortCount();

    std::vector<sai_object_id_t> portList;

    portList.resize(portCount);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list = portList.data();

    /*
     * NOTE: We assume port list is always returned in the same order.
     */

    sai_status_t status = sai_metadata_sai_switch_api->get_switch_attribute(m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to get port list: %s",
                sai_serialize_status(status).c_str());
    }

    portList.resize(attr.value.objlist.count);

    SWSS_LOG_DEBUG("number of ports: %zu", portList.size());

    return portList;
}

std::unordered_map<sai_uint32_t, sai_object_id_t> SaiSwitch::saiGetHardwareLaneMap()
{
    SWSS_LOG_ENTER();

    std::unordered_map<sai_uint32_t, sai_object_id_t> map;

    const std::vector<sai_object_id_t> portList = saiGetPortList();

    /*
     * NOTE: Currently we don't support port breakout this will need to be
     * addressed in future.
     */

    const int lanesPerPort = 4;

    for (const auto &port_rid : portList)
    {
        sai_uint32_t lanes[lanesPerPort];

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = lanesPerPort;
        attr.value.u32list.list = lanes;

        sai_status_t status = sai_metadata_sai_port_api->get_port_attribute(port_rid, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("failed to get hardware lane list port RID %s: %s",
                    sai_serialize_object_id(port_rid).c_str(),
                    sai_serialize_status(status).c_str());
        }

        sai_int32_t laneCount = attr.value.u32list.count;

        for (int j = 0; j < laneCount; j++)
        {
            map[lanes[j]] = port_rid;
        }
    }

    return map;
}

std::string SaiSwitch::getRedisLanesKey()
{
    SWSS_LOG_ENTER();

    /*
     * Each switch will have it's own lanes in format LANES:oid:0xYYYYYYYY.
     *
     * NOTE: To support multple switches LANES needs to be made per switch.
     *
     * return std::string(LANES) + ":" + sai_serialize_object_id(m_switch_vid);
     */

    return std::string(LANES);
}

void SaiSwitch::redisClearLaneMap()
{
    SWSS_LOG_ENTER();

    auto key = getRedisLanesKey();

    g_redisClient->del(key);
}

std::unordered_map<sai_uint32_t, sai_object_id_t> SaiSwitch::redisGetLaneMap()
{
    SWSS_LOG_ENTER();

    auto key = getRedisLanesKey();

    auto hash = g_redisClient->hgetall(key);

    SWSS_LOG_DEBUG("previous lanes: %lu", hash.size());

    std::unordered_map<sai_uint32_t, sai_object_id_t> map;

    for (auto &kv: hash)
    {
        const std::string &str_key = kv.first;
        const std::string &str_value = kv.second;

        sai_uint32_t lane;
        sai_object_id_t portId;

        sai_deserialize_number(str_key, lane);

        sai_deserialize_object_id(str_value, portId);

        map[lane] = portId;
    }

    return map;
}

void SaiSwitch::redisSaveLaneMap(
        _In_ const std::unordered_map<sai_uint32_t, sai_object_id_t> &map)
{
    SWSS_LOG_ENTER();

    redisClearLaneMap();

    for (auto const &it: map)
    {
        sai_uint32_t lane = it.first;
        sai_object_id_t portId = it.second;

        std::string strLane = sai_serialize_number(lane);
        std::string strPortId = sai_serialize_object_id(portId);

        auto key = getRedisLanesKey();

        g_redisClient->hset(key, strLane, strPortId);
    }
}

std::unordered_map<sai_object_id_t, sai_object_id_t> SaiSwitch::redisGetObjectMap(
        _In_ const std::string &key)
{
    SWSS_LOG_ENTER();

    auto hash = g_redisClient->hgetall(key);

    std::unordered_map<sai_object_id_t, sai_object_id_t> map;

    for (auto &kv: hash)
    {
        const std::string &str_key = kv.first;
        const std::string &str_value = kv.second;

        sai_object_id_t objectIdKey;
        sai_object_id_t objectIdValue;

        sai_deserialize_object_id(str_key, objectIdKey);

        sai_deserialize_object_id(str_value, objectIdValue);

        map[objectIdKey] = objectIdValue;
    }

    return map;
}

std::unordered_map<sai_object_id_t, sai_object_id_t> SaiSwitch::redisGetVidToRidMap()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: To support multiple switches VIDTORID must be per switch.
     */

    return redisGetObjectMap(VIDTORID);
}

std::unordered_map<sai_object_id_t, sai_object_id_t> SaiSwitch::redisGetRidToVidMap()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: To support multiple switches RIDTOVID must be per switch.
     */

    return redisGetObjectMap(RIDTOVID);
}

void SaiSwitch::helperCheckLaneMap()
{
    SWSS_LOG_ENTER();

    auto redisLaneMap = redisGetLaneMap();

    auto laneMap = saiGetHardwareLaneMap();

    if (redisLaneMap.size() == 0)
    {
        SWSS_LOG_INFO("no lanes defined in redis, seems like it is first syncd start");

        redisSaveLaneMap(laneMap);

        redisLaneMap = laneMap; // copy
    }

    if (laneMap.size() != redisLaneMap.size())
    {
        SWSS_LOG_THROW("lanes map size differ: %lu vs %lu", laneMap.size(), redisLaneMap.size());
    }

    for (auto kv: laneMap)
    {
        sai_uint32_t lane = kv.first;
        sai_object_id_t portId = kv.second;

        if (redisLaneMap.find(lane) == redisLaneMap.end())
        {
            SWSS_LOG_THROW("lane %u not found in redis", lane);
        }

        if (redisLaneMap[lane] != portId)
        {
            // if this happens, we need to remap VIDTORID and RIDTOVID
            SWSS_LOG_THROW("FIXME: lane port id differs: %s vs %s, port ids must be remapped",
                    sai_serialize_object_id(portId).c_str(),
                    sai_serialize_object_id(redisLaneMap[lane]).c_str());
        }
    }
}

std::string SaiSwitch::getRedisHiddenKey()
{
    SWSS_LOG_ENTER();

    /*
     * Each switch will have it's own hidden in format HIDDEN:oid:0xYYYYYYYY.
     *
     * NOTE: To support multiple switches HIDDEN needs to me made per switch.
     *
     * return std::string(HIDDEN) + ":" + sai_serialize_object_id(m_switch_vid);
     */

    return std::string(HIDDEN);
}

void SaiSwitch::redisCreateRidAndVidMapping(
        _In_ sai_object_id_t rid,
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    std::string strRid = sai_serialize_object_id(rid);
    std::string strVid = sai_serialize_object_id(vid);

    /*
     * TODO: this must be ATOMIC.
     */

    /*
     * NOTE: To support multiple switches VIDTORID and RIDTOVID must be per
     * switch.
     */

    g_redisClient->hset(VIDTORID, strVid, strRid);
    g_redisClient->hset(RIDTOVID, strRid, strVid);

    SWSS_LOG_DEBUG("set VID 0x%lx and RID 0x%lx", vid, rid);
}

void SaiSwitch::redisSetDummyAsicStateForRealObjectId(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType = sai_object_type_query(rid);

    if (objectType == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("sai_object_type_query returned NULL type for RID: %s",
                sai_serialize_object_id(rid).c_str());
    }

    std::string strObjectType = sai_serialize_object_type(objectType);

    sai_object_id_t vid = redis_create_virtual_object_id(m_switch_vid, objectType);

    std::string strVid = sai_serialize_object_id(vid);

    std::string strKey = ASIC_STATE_TABLE + (":" + strObjectType + ":" + strVid);

    g_redisClient->hset(strKey, "NULL", "NULL");

    redisCreateRidAndVidMapping(rid, vid);
}

void SaiSwitch::redisCreateDummyEntryInAsicView(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t vid = translate_rid_to_vid(objectId, m_switch_vid);

    sai_object_type_t objectType = sai_object_type_query(objectId);

    if (objectType == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("sai_object_type_query returned NULL type for RID: %s",
                sai_serialize_object_id(objectId).c_str());
    }

    std::string strObjectType = sai_serialize_object_type(objectType);

    std::string strVid = sai_serialize_object_id(vid);

    std::string strKey = ASIC_STATE_TABLE + (":" + strObjectType + ":" + strVid);

    g_redisClient->hset(strKey, "NULL", "NULL");
}

void SaiSwitch::helperCheckPortIds()
{
    SWSS_LOG_ENTER();

    auto laneMap = saiGetHardwareLaneMap();

    for (auto kv: laneMap)
    {
        sai_object_id_t port_rid = kv.second;

        /*
         * NOTE: Translate will create entry if missing, we assume here that
         * port numbers didn't changed during restarts.
         */

        redisCreateDummyEntryInAsicView(port_rid);

        m_default_ports_rids.insert(port_rid);
    }
}

sai_uint32_t SaiSwitch::saiGetPortNumberOfQueues(
        _In_ sai_object_id_t port_rid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;

    sai_status_t status = sai_metadata_sai_port_api->get_port_attribute(port_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to get port RID %s number of queues: %s",
                sai_serialize_object_id(port_rid).c_str(),
                sai_serialize_status(status).c_str());
    }

    SWSS_LOG_DEBUG("port RID %s queues number: %u",
            sai_serialize_object_id(port_rid).c_str(),
            attr.value.u32);

    return attr.value.u32;
}

std::vector<sai_object_id_t> SaiSwitch::saiGetPortQueues(
        _In_ sai_object_id_t port_rid)
{
    SWSS_LOG_ENTER();

    uint32_t queueCount = saiGetPortNumberOfQueues(port_rid);

    std::vector<sai_object_id_t> queueList;

    if (queueCount == 0)
    {
        return queueList;
    }

    queueList.resize(queueCount);

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist.count = queueCount;
    attr.value.objlist.list = queueList.data();

    sai_status_t status = sai_metadata_sai_port_api->get_port_attribute(port_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to get port RID %s queue list: %s",
                sai_serialize_object_id(port_rid).c_str(),
                sai_serialize_status(status).c_str());
    }

    queueList.resize(attr.value.objlist.count);

    return queueList;
}

void SaiSwitch::helperCheckQueuesIds()
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> ports = saiGetPortList();

    for (const auto& port_rid: ports)
    {
        SWSS_LOG_DEBUG("getting queues for port RID: %s",
                sai_serialize_object_id(port_rid).c_str());

        std::vector<sai_object_id_t> queues = saiGetPortQueues(port_rid);

        /*
         * We have queues.
         */

        for (const auto& queue_rid: queues)
        {
            /*
             * NOTE: Create entry in asic view if missing we assume here that
             * queue numbers will not be changed during restarts.
             */

            redisCreateDummyEntryInAsicView(queue_rid);

            m_default_queues_rids.insert(queue_rid);
        }
    }
}

sai_uint32_t SaiSwitch::saiGetPortNumberOfPriorityGroups(
        _In_ sai_object_id_t port_rid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS;

    sai_status_t status = sai_metadata_sai_port_api->get_port_attribute(port_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to get port RID %s number of priority groups: %s",
                sai_serialize_object_id(port_rid).c_str(),
                sai_serialize_status(status).c_str());
    }

    SWSS_LOG_DEBUG("port RID %s priority groups number: %u",
            sai_serialize_object_id(port_rid).c_str(),
            attr.value.u32);

    return attr.value.u32;
}

std::vector<sai_object_id_t> SaiSwitch::saiGetPortPriorityGroups(
        _In_ sai_object_id_t port_rid)
{
    SWSS_LOG_ENTER();

    uint32_t pgCount = saiGetPortNumberOfPriorityGroups(port_rid);

    std::vector<sai_object_id_t> pgList;

    if (pgCount == 0)
    {
        return pgList;
    }

    pgList.resize(pgCount);

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST;
    attr.value.objlist.count = pgCount;
    attr.value.objlist.list = pgList.data();

    sai_status_t status = sai_metadata_sai_port_api->get_port_attribute(port_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to get port RID %s priority groups list: %s",
                sai_serialize_object_id(port_rid).c_str(),
                sai_serialize_status(status).c_str());
    }

    pgList.resize(attr.value.objlist.count);

    return pgList;
}

void SaiSwitch::helperCheckPriorityGroupsIds()
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> ports = saiGetPortList();

    for (const auto& port_rid: ports)
    {
        SWSS_LOG_DEBUG("getting priority groups for port RID: %s",
                sai_serialize_object_id(port_rid).c_str());

        std::vector<sai_object_id_t> pgs = saiGetPortPriorityGroups(port_rid);

        for (const auto& pg_rid: pgs)
        {
            /*
             * NOTE: Create entry in asic view if missing we assume here that
             * PG numbers will not be changed during restarts.
             */

            redisCreateDummyEntryInAsicView(pg_rid);

            m_default_priority_groups_rids.insert(pg_rid);
        }
    }
}

uint32_t SaiSwitch::saiGetMaxNumberOfChildsPerSchedulerGroup()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP;

    sai_status_t status = sai_metadata_sai_switch_api->get_switch_attribute(m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("faled to obtain SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP for switch RID %s: %s",
                sai_serialize_object_id(m_switch_rid).c_str(),
                sai_serialize_status(status).c_str());
    }

    return attr.value.u32;
}

uint32_t SaiSwitch::saiGetQosNumberOfSchedulerGroups(
        _In_ sai_object_id_t port_rid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS;

    sai_status_t status = sai_metadata_sai_port_api->get_port_attribute(port_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("Failed to get number of scheduler groups for port RID %s: %s",
                sai_serialize_object_id(port_rid).c_str(),
                sai_serialize_status(status).c_str());
    }

    /*
     * Total groups list on the port.
     */

    return attr.value.u32;
}

std::vector<sai_object_id_t> SaiSwitch::saiGetSchedulerGroupList(
        _In_ sai_object_id_t port_rid)
{
    SWSS_LOG_ENTER();

    uint32_t groupsCount = saiGetQosNumberOfSchedulerGroups(port_rid);

    std::vector<sai_object_id_t> groups;

    groups.resize(groupsCount);

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST;
    attr.value.objlist.list = groups.data();
    attr.value.objlist.count = (uint32_t)groups.size();

    sai_status_t status = sai_metadata_sai_port_api->get_port_attribute(port_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("Failed to get scheduler group list for port RID %s: %s",
                sai_serialize_object_id(port_rid).c_str(),
                sai_serialize_status(status).c_str());
    }

    groups.resize(attr.value.objlist.count);

    SWSS_LOG_DEBUG("got %zu scheduler groups on port RID %s",
            groups.size(),
            sai_serialize_object_id(port_rid).c_str());

    return groups;
}

uint32_t SaiSwitch::saiGetSchedulerGroupChildCount(
        _In_ sai_object_id_t sg_rid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT; // queues + sched groups

    sai_status_t status = sai_metadata_sai_scheduler_group_api->get_scheduler_group_attribute(sg_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("Failed to get child count for scheduler group RID %s: %s",
                sai_serialize_object_id(sg_rid).c_str(),
                sai_serialize_status(status).c_str());
    }

    return attr.value.u32;
}

std::vector<sai_object_id_t> SaiSwitch::saiGetSchedulerGroupChildList(
        _In_ sai_object_id_t sg_rid)
{
    SWSS_LOG_ENTER();

    uint32_t childCount = saiGetSchedulerGroupChildCount(sg_rid);

    SWSS_LOG_DEBUG("child count for SG RID %s is %u",
            sai_serialize_object_id(sg_rid).c_str(),
            childCount);

    std::vector<sai_object_id_t> childs;

    childs.resize(childCount);

    sai_attribute_t attr;

    attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
    attr.value.objlist.list = childs.data();
    attr.value.objlist.count = (uint32_t)childs.size();

    sai_status_t status = sai_metadata_sai_scheduler_group_api->get_scheduler_group_attribute(sg_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("Failed to get child list for scheduler group RID %s: %s",
                sai_serialize_object_id(sg_rid).c_str(),
                sai_serialize_status(status).c_str());
    }

    childs.resize(attr.value.objlist.count);

    SWSS_LOG_DEBUG("got %zu childs on scheduler group RID %s",
            childs.size(),
            sai_serialize_object_id(sg_rid).c_str());

    return childs; // scheduler groups + queues
}

void SaiSwitch::helperCheckSchedulerGroupsIds()
{
    SWSS_LOG_ENTER();

    for (const auto& port_rid: saiGetPortList())
    {
        /*
         * Each group can contain next scheduler group or queue.
         */

        for (const auto& sg_rid: saiGetSchedulerGroupList(port_rid))
        {
            /*
             * NOTE: Create entry in asic view if missing we assume here that
             * SchedulerGroups numbers will not be changed during restarts.
             */

            redisCreateDummyEntryInAsicView(sg_rid);

            m_default_scheduler_groups_rids.insert(sg_rid);
        }
    }
}

sai_object_id_t SaiSwitch::getVid() const
{
    SWSS_LOG_ENTER();

    return m_switch_vid;
}

sai_object_id_t SaiSwitch::getRid() const
{
    SWSS_LOG_ENTER();

    return m_switch_rid;
}

std::string SaiSwitch::getHardwareInfo() const
{
    SWSS_LOG_ENTER();

    /*
     * Since this attribute is CREATE_ONLY this value will not change in entire
     * life of switch.
     */

    return m_hardware_info;
}

#define MAX_VLAN_MEMBERS 0x10000

void SaiSwitch::removeDefaultVlanMembers()
{
    SWSS_LOG_ENTER();

    /*
     * In sai v1.0 vlan members are not by port but
     * by bridge port id.
     */

    /*
     * TODO: This method needs to be revisited after asic init in v1.0, all
     * bridge ports are vlan 1 members, we will remove them to not complicate
     * reinit if user want to add bridge ports to vlan 1, then it needs to be
     * done explicitly.
     */

    std::vector<sai_object_id_t> vlanMemberList;

    vlanMemberList.resize(MAX_VLAN_MEMBERS);

    sai_attribute_t attr;

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;

    attr.value.objlist.count = (uint32_t)vlanMemberList.size();
    attr.value.objlist.list = vlanMemberList.data();

    sai_object_id_t default_vlan_rid =
        getSwitchDefaultAttrOid(SAI_SWITCH_ATTR_DEFAULT_VLAN_ID);

    sai_status_t status = sai_metadata_sai_vlan_api->get_vlan_attribute(default_vlan_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to obtain default vlan %s members",
                sai_serialize_object_id(default_vlan_rid).c_str());
    }

    vlanMemberList.resize(attr.value.objlist.count);

    SWSS_LOG_DEBUG("obtained %zu vlan members", vlanMemberList.size());

    for (auto &vm: vlanMemberList)
    {
        status = sai_metadata_sai_vlan_api->remove_vlan_member(vm);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("Failed to remove vlan member RID %s from vlan RID %s",
                    sai_serialize_object_id(vm).c_str(),
                    sai_serialize_object_id(default_vlan_rid).c_str());
        }
    }
}

void SaiSwitch::buildNonCreateRids()
{
    SWSS_LOG_ENTER();

    for (const auto &p: m_default_rid_map)
    {
        m_non_create_rids.insert(p.second);
    }

    // TODO we need to use Table to put everything after hard reinit
    // and we need flag to

    // TODO we need support for remove objects that are no more present
    // and we need to start removing them basing on references count
    // we cant remove bridge before bridge port

    // TODO get all this stuff from discovery !

    m_non_create_rids.insert(m_default_priority_groups_rids.begin(), m_default_priority_groups_rids.end());
    m_non_create_rids.insert(m_default_queues_rids.begin(), m_default_queues_rids.end());
    m_non_create_rids.insert(m_default_scheduler_groups_rids.begin(), m_default_scheduler_groups_rids.end());
    m_non_create_rids.insert(m_default_ports_rids.begin(), m_default_ports_rids.end());
}

bool SaiSwitch::isNonCreateRid(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    return m_non_create_rids.find(rid) != m_non_create_rids.end();
}

std::set<sai_object_id_t> SaiSwitch::getExistingObjects() const
{
    SWSS_LOG_ENTER();

    return m_non_create_rids;
}

std::vector<sai_port_stat_t> SaiSwitch::saiGetSupportedCounters()
{
    SWSS_LOG_ENTER();

    auto ports = saiGetPortList();

    if (ports.size() == 0)
    {
        SWSS_LOG_THROW("no ports are defined on switch");
    }

    sai_object_id_t port_rid = ports.at(0);

    std::vector<sai_port_stat_t> supportedCounters;

    for (uint32_t idx = 0; idx < metadata_enum_sai_port_stat_t.valuescount; ++idx)
    {
        sai_port_stat_t counter = (sai_port_stat_t)metadata_enum_sai_port_stat_t.values[idx];

        uint64_t value;

        sai_status_t status = sai_metadata_sai_port_api->get_port_stats(port_rid, &counter, 1, &value);

        if (status != SAI_STATUS_SUCCESS)
        {
            const std::string &name = sai_serialize_port_stat(counter);

            SWSS_LOG_INFO("counter %s is not supported on port RID %s: %s",
                    name.c_str(),
                    sai_serialize_object_id(port_rid).c_str(),
                    sai_serialize_status(status).c_str());

            continue;
        }

        supportedCounters.push_back(counter);
    }

    SWSS_LOG_NOTICE("supported %zu of %d",
            supportedCounters.size(),
            metadata_enum_sai_port_stat_t.valuescount);

    return supportedCounters;
}

void SaiSwitch::collectCounters(
        _In_ swss::Table &countersTable)
{
    SWSS_LOG_ENTER();

    if (m_supported_counters.size() == 0)
    {
        /*
         * There are not supported counters :(
         */

        return;
    }

    SWSS_LOG_TIMER("get counters");

    uint32_t countersSize = (uint32_t)m_supported_counters.size();

    std::vector<uint64_t> counters;

    counters.resize(countersSize);

    auto ports = saiGetPortList();

    for (auto &port_rid: ports)
    {
        sai_status_t status = sai_metadata_sai_port_api->get_port_stats(
                port_rid,
                m_supported_counters.data(),
                countersSize,
                counters.data());

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to collect counters for port RID %s: %s",
                    sai_serialize_object_id(port_rid).c_str(),
                    sai_serialize_status(status).c_str());
            continue;
        }

        sai_object_id_t vid = translate_rid_to_vid(port_rid, m_switch_vid);

        std::string strPortId = sai_serialize_object_id(vid);

        // for counters, use port vid as printf "%llx" format
        std::stringstream ss;
        ss << std::hex << vid;
        strPortId = ss.str();

        std::vector<swss::FieldValueTuple> values;

        for (size_t idx = 0; idx < counters.size(); idx++)
        {
            const std::string &field = sai_serialize_port_stat(m_supported_counters[idx]);
            const std::string &value = std::to_string(counters[idx]);

            swss::FieldValueTuple fvt(field, value);

            values.push_back(fvt);
        }

        countersTable.set(strPortId, values, "");
    }
}

/**
 * @brief Helper function to get attribute oid from switch.
 *
 * Helper will try to obtain oid value for given attribute id.  On success, it
 * will try to obtain this value from redis database.  When value is not in
 * redis yet, it will store it, but when value was already there, it will
 * compare redis value to current oid and when they are different, it will
 * throw exception requesting for fix. When oid values are equal, function
 * returns current value.
 *
 * @param attr_id Attribute id to obtain oid from it.
 *
 * @return Valid object id (riD) if present, SAI_NULL_OBJECT_ID on failure.
 */
sai_object_id_t SaiSwitch::helperGetSwitchAttrOid(
        _In_ sai_attr_id_t attr_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    /*
     * Get original value from the ASIC.
     */

    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr_id);

    attr.id = attr_id;

    sai_status_t status = sai_metadata_sai_switch_api->get_switch_attribute(m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get %s: %s",
                meta->attridname,
                sai_serialize_status(status).c_str());

        m_default_rid_map[attr_id] = SAI_NULL_OBJECT_ID;

        return SAI_NULL_OBJECT_ID;
    }

    SWSS_LOG_DEBUG("%s RID %s",
            meta->attridname,
            sai_serialize_object_id(attr.value.oid).c_str());

    sai_object_id_t rid = attr.value.oid;

    sai_object_id_t redis_rid = SAI_NULL_OBJECT_ID;

    /*
     * Get value value of the same attribute from redis.
     */

    auto key = getRedisHiddenKey();

    auto ptr_redis_rid_str = g_redisClient->hget(key, meta->attridname);

    if (ptr_redis_rid_str == NULL)
    {
        /*
         * Redis value of this attribute is not present yet, save it!
         */

        redisSetDummyAsicStateForRealObjectId(rid);

        SWSS_LOG_INFO("redis %s id is not defined yet in redis", meta->attridname);

        std::string rid_str = sai_serialize_object_id(rid);

        g_redisClient->hset(key, meta->attridname, rid_str);

        m_default_rid_map[attr_id] = rid;

        return rid;
    }

    sai_deserialize_object_id(*ptr_redis_rid_str, redis_rid);

    if (rid != redis_rid)
    {
        /*
         * In that case we need to remap VIDTORID and RIDTOVID. This is
         * required since if previous value will be used in redis maps, and it
         * will be invalid when that value will be used to call SAI api.
         */

        SWSS_LOG_THROW("FIXME: %s RID differs: %s (asic) vs %s (redis), ids must be remapped v2r/r2v",
                meta->attridname,
                sai_serialize_object_id(rid).c_str(),
                sai_serialize_object_id(redis_rid).c_str());
    }

    m_default_rid_map[attr_id] = rid;

    return rid;
}

sai_object_id_t SaiSwitch::getSwitchDefaultAttrOid(
        _In_ sai_object_id_t attr_id) const
{
    SWSS_LOG_ENTER();

    auto it = m_default_rid_map.find(attr_id);

    if (it == m_default_rid_map.end())
    {
        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr_id);

        const char* name = (meta) ? meta->attridname : "UNKNOWN";

        SWSS_LOG_THROW("attribute %s (%d) not found in default RID map", name, attr_id);
    }

    return it->second;
}

void SaiSwitch::saiDiscover(
        _In_ sai_object_id_t rid,
        _Inout_ std::set<sai_object_id_t> &discovered)
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: This method is only good after switch init
     * since we are making assumptions that tere are no
     * ACL after initialization.
     */

    if (rid == SAI_NULL_OBJECT_ID)
    {
        return;
    }

    if (discovered.find(rid) != discovered.end())
    {
        return;
    }

    discovered.insert(rid);

    sai_object_type_t ot = sai_object_type_query(rid);

    SWSS_LOG_DEBUG("processing %s: %s",
            sai_serialize_object_id(rid).c_str(),
            sai_serialize_object_type(ot).c_str());

    if (ot == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_ERROR("id %s returned NULL object type",
                sai_serialize_object_id(rid).c_str());

        return;
    }

    // TODO later use sai_metadata_get_object_type_info(ot);
    const sai_object_type_info_t *info =  sai_all_object_type_infos[ot];

    /*
     * We will query only oid object types
     * then we don't need meta key, but we need to add to metadata
     * pointers to only generic functions.
     */

    sai_object_meta_key_t mk = { .objecttype = ot, .objectkey = { .key = { .object_id = rid } } };

    for (int idx = 0; info->attrmetadata[idx] != NULL; ++idx)
    {
        const sai_attr_metadata_t *md = info->attrmetadata[idx];

        /*
         * Note that we don't care about ACL object id's since
         * we assume that there are no ACLs on switch after init.
         */

        sai_attribute_t attr;

        attr.id = md->attrid;

        if (md->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            if (md->defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_CONST)
            {
                /*
                 * This means that default value for this object is
                 * SAI_NULL_OBJECT_ID, since this is discovery after
                 * create, we don't need to query this attribute.
                 */

                continue;
            }

            if (md->objecttype == SAI_OBJECT_TYPE_STP &&
                    md->attrid == SAI_STP_ATTR_BRIDGE_ID)
            {
                SWSS_LOG_ERROR("skipping since it causes crash: %s", md->attridname);
                continue;
            }

            if (md->objecttype == SAI_OBJECT_TYPE_BRIDGE_PORT)
            {
                if (md->attrid == SAI_BRIDGE_PORT_ATTR_TUNNEL_ID ||
                        md->attrid == SAI_BRIDGE_PORT_ATTR_RIF_ID)
                {
                    /*
                     * We know that bridge port is binded on PORT, no need
                     * to query those attributes.
                     */

                    continue;
                }
            }

            SWSS_LOG_DEBUG("getting %s for %s", md->attridname,
                    sai_serialize_object_id(rid).c_str());

            sai_status_t status = info->get(&mk, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                /*
                 * We failed to get value, maybe it's not supported ?
                 */

                SWSS_LOG_WARN("%s: %s on %s",
                        md->attridname,
                        sai_serialize_status(status).c_str(),
                        sai_serialize_object_id(rid).c_str());

                continue;
            }

            saiDiscover(attr.value.oid, discovered); // recursion
        }
        else if (md->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_LIST)
        {
            if (md->defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST)
            {
                /*
                 * This means that default value for this object is
                 * empty list, since this is discovery after
                 * create, we don't need to query this attribute.
                 */

                continue;
            }

            SWSS_LOG_DEBUG("getting %s for %s", md->attridname,
                    sai_serialize_object_id(rid).c_str());

            sai_object_id_t local[SAI_DISCOVERY_LIST_MAX_ELEMENTS];

            attr.value.objlist.count = SAI_DISCOVERY_LIST_MAX_ELEMENTS;
            attr.value.objlist.list = local;

            sai_status_t status = info->get(&mk, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                /*
                 * We failed to get value, maybe it's not supported ?
                 */

                SWSS_LOG_WARN("%s: %s on %s",
                        md->attridname,
                        sai_serialize_status(status).c_str(),
                        sai_serialize_object_id(rid).c_str());

                continue;
            }

            SWSS_LOG_DEBUG("list count %s %u", md->attridname, attr.value.objlist.count);

            for (uint32_t i = 0; i < attr.value.objlist.count; ++i)
            {
                saiDiscover(attr.value.objlist.list[i], discovered); // recursion
            }
        }
    }
}

void SaiSwitch::helperDiscover()
{
    SWSS_LOG_ENTER();

    /*
     * Preform discovery on the switch to obtain ASIC view of
     * objects that are created internally.
     */

    m_discovered_rids.clear();

    {
        SWSS_LOG_TIMER("discover");

        saiDiscover(m_switch_rid, m_discovered_rids);
    }

    std::map<sai_object_type_t,int> map;

    for (sai_object_id_t rid: m_discovered_rids)
    {
        map[sai_object_type_query(rid)]++;
    }

    for (const auto &p: map)
    {
        SWSS_LOG_NOTICE("%s: %d", sai_serialize_object_type(p.first).c_str(), p.second);
    }
}

SaiSwitch::SaiSwitch(
        _In_ sai_object_id_t switch_vid,
        _In_ sai_object_id_t switch_rid)
{
    SWSS_LOG_ENTER();

    m_switch_rid = switch_rid;
    m_switch_vid = switch_vid;

    m_hardware_info = saiGetHardwareInfo();

    // TODO this can be also automated based on metadata
    // to get attributes read-only and with value "internal"

    helperGetSwitchAttrOid(SAI_SWITCH_ATTR_CPU_PORT);
    helperGetSwitchAttrOid(SAI_SWITCH_ATTR_DEFAULT_VLAN_ID);
    helperGetSwitchAttrOid(SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID);
    helperGetSwitchAttrOid(SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP);
    helperGetSwitchAttrOid(SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID);
    helperGetSwitchAttrOid(SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID);

    helperCheckLaneMap();

    helperDiscover();
    // TODO when veryfirst run, then we can't put them to redis
    // since we would override some objects if some needs to be removed
    // but this only in hard reinit, then we have some objects (probably some
    // removed like vlan members) and then they exists in switch
    // so we can't put them to db, and we will need to remove them

    helperCheckPortIds();

    helperCheckQueuesIds();

    helperCheckPriorityGroupsIds();

    helperCheckSchedulerGroupsIds();

    // TODO we will need bridge ports here also.
    // TODO when doing hard reinit we need to take care of that (non create)
    // TODO extra handling needs to be done since they must be matched
    // by port Id and not by lanes, this will be non trivial
    //
    // matching oid's should be based on dependency tree, so it's not enough
    // just to keep oids, but we need to keep map and check vs asic view

    buildNonCreateRids();

    m_supported_counters = saiGetSupportedCounters();

    /*
     * We remove default vlan members to make it easier to reinit since our
     * orch agent will remove them anyway.
     *
     * NOTE: To make this fully functional we need make use of method that
     * will list all objects after switch creation.
     */

    removeDefaultVlanMembers();
}
