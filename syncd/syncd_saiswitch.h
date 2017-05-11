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

        bool isDefaultCreatedRid(
                _In_ sai_object_id_t rid);

        void collectCounters(
                _In_ swss::Table &countersTable);

        /*
         * Redis Static Methods.
         */

        static std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetObjectMap(
                _In_ const std::string &key);

        /**
         * @brief Gets existing objects on the switch.
         *
         * This value can be corrected, when for example default VLAN members
         * will be removed.
         *
         * @returns Default existing objects on the switch.
         */
        std::set<sai_object_id_t> getExistingObjects() const;

        /**
         * @brief Gets default object based on switch attribute.
         *
         * NOTE: This method will throw exception if invalid attribute is
         * specified, since attribute queried by this method are explicitly
         * declared in SaiSwitch constructor.
         *
         * @param attr_id Switch attrbute to query.
         *
         * @return Valid RID or specified switch attribute received from
         * switch.  This value can be also SAI_NULL_OBJECT_ID if switch don't
         * support this attribute.
         */
        sai_object_id_t getSwitchDefaultAttrOid(
                _In_ sai_object_id_t attr_id) const;

        /**
         * @brief Remove existing object from the switch.
         *
         * Function throws when object can't be removed.
         *
         * @param rid Real object ID.
         */
        void removeExistingObject(
                _In_ sai_object_id_t rid);

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

        void redisSetDummyAsicStateForRealObjectId(
                _In_ sai_object_id_t rid);

        /*
         * Helper Methods.
         */

        void helperCheckLaneMap();

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

        void helperPutDiscoveredRidsToRedis();

        /*
         * Other Methods.
         */

        std::string getRedisLanesKey();
        std::string getRedisHiddenKey();
};

extern std::map<sai_object_id_t, std::shared_ptr<SaiSwitch>> switches;

#endif // __SYNCD_SAISWITCH_H__
