#ifndef __SYNCD_SAISWITCH_H__
#define __SYNCD_SAISWITCH_H__

extern "C" {
#include "sai.h"
}

/**
 * @def SAI_DISCOVERY_LIST_MAX_ELEMENTS
 *
 * Defines maximum elements that can be obtained from the OID list when
 * performing list attribute query (discovery) on the switch.
 *
 * This vlaue will be used to allocate memory on the stack for obtaining object
 * list, and should be big enough to obtain list for all ports on the switch
 * and vlan members.
 */
#define SAI_DISCOVERY_LIST_MAX_ELEMENTS 1024

class SaiSwitch
{
    public:

        /*
         * Constructors.
         */

        SaiSwitch(
                _In_ sai_object_id_t switch_vid,
                _In_ sai_object_id_t switch_rid);

        /*
         * Methods.
         */

        sai_object_id_t getVid() const;
        sai_object_id_t getRid() const;

        std::string getHardwareInfo() const;

        void removeDefaultVlanMembers();

        bool isNonCreateRid(
                _In_ sai_object_id_t rid);

        void collectCounters(
                _In_ swss::Table &countersTable);

        /*
         * Redis Static Methods.
         */

        static std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetObjectMap(
                _In_ const std::string &key);

        /**
         * @brief Gets existing objects on the switch
         *
         * @returns Default existing objects on the switch
         */
        std::set<sai_object_id_t> getExistingObjects() const;

        /**
         * @brief Gets default object based on switch attribute.
         *
         * NOTE: This method will throw exception if invalid attribute is
         * specified, since attribute queried by this method are explicitly
         * declared in SaiSwitch constructor.
         *
         * @param attr_id Attrbute to query
         *
         * @return Valid RID or specified switch attribute received from
         * switch.  This value can be also SAI_NULL_OBJECT_ID if switch don't
         * support this attribute.
         */
        sai_object_id_t getSwitchDefaultAttrOid(
                _In_ sai_object_id_t attr_id) const;

    private:

        /*
         * Constructors.
         */

        SaiSwitch(const SaiSwitch&);

        /*
         * Operators.
         */

        SaiSwitch& operator=(const SaiSwitch&);

        /*
         * Fields.
         */

        /**
         * @brief Switch virtual ID assigned by syncd.
         */
        sai_object_id_t m_switch_vid;

        /**
         * @brief Swtich real ID assigned by SAI SDK.
         */
        sai_object_id_t m_switch_rid;

        std::string m_hardware_info;

        std::vector<sai_port_stat_t> m_supported_counters;

        /*
         * NOTE: Those default value will make sense only when we will do hard
         * reinit, since when doing warm restart syncd will restart but if for
         * example we removed some scheduler groups or added/removed ports,
         * those numbers won't match and we will throw.
         *
         * For that case we need special handling. We will implement that
         * later, when this scenario will happen.
         */

        /**
         * @brief Map of default RIDs retrived from Switch object.
         *
         * It will contain RIDs like CPU port, default virtual router, default
         * trap group. etc. Those objects here should be considered non
         * removable.
         */
        std::map<sai_attr_id_t,sai_object_id_t> m_default_rid_map;

        /*
         * NOTE: Later we need to have this in redis with port mapping.
         */

        std::set<sai_object_id_t> m_default_priority_groups_rids;
        std::set<sai_object_id_t> m_default_queues_rids;
        std::set<sai_object_id_t> m_default_scheduler_groups_rids;
        std::set<sai_object_id_t> m_default_ports_rids;

        /**
         * @brief Objects existing on the switch after creating switch
         *
         * Includes CPU port, ports, queues, etc.
         *
         * Name is confusing, since "non create rids" means that those are RIDs
         * that were not created by user.
         */
        std::set<sai_object_id_t> m_non_create_rids;

        /**
         * @brief Discovered objects.
         *
         * Set of object IDs discovered after calling saiDiscovery method.
         * This set will contain all objects present on the switch right after
         * switch init.
         */
        std::set<sai_object_id_t> m_discovered_rids;

        /*
         * SAI Methods.
         */

        sai_uint32_t saiGetPortCount();

        std::string saiGetHardwareInfo();

        std::vector<sai_object_id_t> saiGetPortList();

        std::unordered_map<sai_uint32_t, sai_object_id_t> saiGetHardwareLaneMap();

        sai_uint32_t saiGetPortNumberOfQueues(
                _In_ sai_object_id_t port_vid);

        std::vector<sai_object_id_t> saiGetPortQueues(
                _In_ sai_object_id_t port_rid);

        sai_uint32_t saiGetPortNumberOfPriorityGroups(
                _In_ sai_object_id_t port_rid);

        std::vector<sai_object_id_t> saiGetPortPriorityGroups(
                _In_ sai_object_id_t port_rid);

        std::vector<sai_object_id_t> saiGetSchedulerGroupList(
                _In_ sai_object_id_t port_rid);

        std::vector<sai_object_id_t> saiGetSchedulerGroupChildList(
                _In_ sai_object_id_t sg_rid);

        uint32_t saiGetSchedulerGroupChildCount(
                _In_ sai_object_id_t sg_rid);

        uint32_t saiGetQosNumberOfSchedulerGroups(
                _In_ sai_object_id_t port_rid);

        uint32_t saiGetMaxNumberOfChildsPerSchedulerGroup();

        std::vector<sai_port_stat_t> saiGetSupportedCounters();

        /*
         * Redis Methods.
         */

        std::unordered_map<sai_uint32_t, sai_object_id_t> redisGetLaneMap();

    public:

        std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetVidToRidMap();

        std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetRidToVidMap();

    private:

        void redisSaveLaneMap(
                _In_ const std::unordered_map<sai_uint32_t, sai_object_id_t> &map);

        void redisClearLaneMap();

        void redisCreateRidAndVidMapping(
                _In_ sai_object_id_t rid,
                _In_ sai_object_id_t vid);

        void redisSetDummyAsicStateForRealObjectId(
                _In_ sai_object_id_t rid);

        void redisCreateDummyEntryInAsicView(
                _In_ sai_object_id_t objectId);

        /*
         * Helper Methods.
         */

        void helperCheckLaneMap();
        void helperCheckPortIds();
        void helperCheckQueuesIds();
        void helperCheckPriorityGroupsIds();
        void helperCheckSchedulerGroupsIds();

        sai_object_id_t helperGetSwitchAttrOid(
                _In_ sai_attr_id_t attr_id);

        /**
         * @brief Discover objects on the swtich.
         *
         * Method will query recursivly all OID attributes (oid and list) on
         * the given object.
         *
         * This method should be called only once inside constructor right
         * after switch has beed breated to obtain actual ASIC view.
         *
         * @param rid Object to discover other objects.
         * @param processed Set of already processed objects. This set will be
         * updated every time new object ID is discovered.
         */
        void saiDiscover(
                _In_ sai_object_id_t rid,
                _Inout_ std::set<sai_object_id_t> &processed);

        /**
         * @brief Discover helper.
         *
         * Method will call saiDiscovery and log all discovered objects.
         */
        void helperDiscover();

        /*
         * Other Methods.
         */

        std::string getRedisLanesKey();
        std::string getRedisHiddenKey();

        void buildNonCreateRids();
};

extern std::map<sai_object_id_t, std::shared_ptr<SaiSwitch>> switches;

#endif // __SYNCD_SAISWITCH_H__
