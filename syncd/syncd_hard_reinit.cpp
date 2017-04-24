#include "syncd.h"
#include "sairedis.h"

#include <string>
#include <vector>
#include <unordered_map>

/*
 * To support multiple switches here we need to refactor this to a class
 * similar to SaiSwitch, and then have separate vid/rid map and ecah class will
 * be recreating only one switch and handling that
 */

typedef std::unordered_map<std::string, std::string> StringHash;
typedef std::unordered_map<sai_object_id_t, sai_object_id_t> ObjectIdMap;

static std::unordered_map<std::string, std::shared_ptr<SaiAttributeList>> g_attributesLists;

static ObjectIdMap g_translated;
static ObjectIdMap g_vidToRidMap;
static ObjectIdMap g_ridToVidMap;

static StringHash g_oids;
static StringHash g_switches;
static StringHash g_fdbs;
static StringHash g_routes;
static StringHash g_neighbors;

/*
 * Since we are only supporting 1 switch we can declare this as global, and we
 * can get away with this, but later on we need to convert this to a class like
 * SaiSwitch.
 */
static sai_object_id_t g_switch_rid = SAI_NULL_OBJECT_ID;
static sai_object_id_t g_switch_vid = SAI_NULL_OBJECT_ID;

static std::shared_ptr<SaiSwitch> g_sw;

void processAttributesForOids(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

sai_object_type_t getObjectTypeFromAsicKey(
        _In_ const std::string &key);

sai_object_type_t getObjectTypeFromVid(
        _In_ sai_object_id_t object_vid)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType = redis_sai_object_type_query(object_vid);

    if (objectType >= SAI_OBJECT_TYPE_MAX ||
            objectType == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("invalid object type: %s on object id: %s",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(object_vid).c_str());
    }

    return objectType;
}

std::shared_ptr<SaiAttributeList> redisGetAttributesFromAsicKey(
        _In_ const std::string &key)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType = getObjectTypeFromAsicKey(key);

    std::vector<swss::FieldValueTuple> values;

    auto hash = g_redisClient->hgetall(key);

    for (auto &kv: hash)
    {
        const std::string &skey = kv.first;
        const std::string &svalue = kv.second;

        swss::FieldValueTuple fvt(skey, svalue);

        values.push_back(fvt);
    }

    return std::make_shared<SaiAttributeList>(objectType, values, false);
}

sai_object_type_t getObjectTypeFromAsicKey(
        _In_ const std::string &key)
{
    SWSS_LOG_ENTER();

    auto start = key.find_first_of(":") + 1;
    auto end = key.find(":", start);

    const std::string strObjectType = key.substr(start, end - start);

    sai_object_type_t objectType;
    sai_deserialize_object_type(strObjectType, objectType);

    if (objectType >= SAI_OBJECT_TYPE_MAX ||
            objectType == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("invalid object type: %s on asic key: %s",
                sai_serialize_object_type(objectType).c_str(),
                key.c_str());
    }

    return objectType;
}

std::string getObjectIdFromAsicKey(
        _In_ const std::string &key)
{
    SWSS_LOG_ENTER();

    auto start = key.find_first_of(":") + 1;
    auto end = key.find(":", start);

    return key.substr(end + 1);
}

void redisClearVidToRidMap()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: needs to be done per switch.
     */

    g_redisClient->del(VIDTORID);
}

void redisClearRidToVidMap()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: needs to be done per switch.
     */

    g_redisClient->del(RIDTOVID);
}

void redisSetVidAndRidMap(
        _In_ const std::unordered_map<sai_object_id_t, sai_object_id_t> &map)
{
    SWSS_LOG_ENTER();

    /*
     * TODO clear can be done after recreating all switches unless vid/rid map
     * will be per switch.
     *
     * This needs to be addressed when we want to support multiple switches.
     */

    redisClearVidToRidMap();
    redisClearRidToVidMap();

    for (auto &kv: map)
    {
        std::string strVid = sai_serialize_object_id(kv.first);
        std::string strRid = sai_serialize_object_id(kv.second);

        g_redisClient->hset(VIDTORID, strVid, strRid);
        g_redisClient->hset(RIDTOVID, strRid, strVid);
    }
}

void checkAllIds()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_translated)
    {
        auto it = g_vidToRidMap.find(kv.first);

        if (it == g_vidToRidMap.end())
        {
            SWSS_LOG_THROW("failed to find vid %s in previous map",
                    sai_serialize_object_id(kv.first).c_str());
        }

        g_vidToRidMap.erase(it);
    }

    size_t size = g_vidToRidMap.size();

    if (size != 0)
    {
        for (auto &kv: g_vidToRidMap)
        {
            sai_object_type_t objectType = getObjectTypeFromVid(kv.first);

            SWSS_LOG_ERROR("vid not translated: %s, object type: %s",
                    sai_serialize_object_id(kv.first).c_str(),
                    sai_serialize_object_type(objectType).c_str());
        }

        SWSS_LOG_THROW("vid to rid map is not empty (%zu) after translation", size);
    }

    redisSetVidAndRidMap(g_translated);
}

std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetVidToRidMap()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: To support multiple switches VIDTORID must be per switch.
     */

    return SaiSwitch::redisGetObjectMap(VIDTORID);
}

std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetRidToVidMap()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: To support multiple switches RIDTOVID must be per switch.
     */

    return SaiSwitch::redisGetObjectMap(RIDTOVID);
}

void processSwitches()
{
    SWSS_LOG_ENTER();

    /*
     * If there are any switches, we need to create them first to perform any
     * other operations.
     *
     * NOTE: This method needs to be revisited if we want to support multiple
     * switches.
     */

    if (switches.size() != 0)
    {
        SWSS_LOG_THROW("performing hard reinit, but there are %zu switches defined, bug!", switches.size());
    }

    if (g_switches.size() > 1)
    {
        SWSS_LOG_THROW("multiple switches %zu in hard reinit are not supported yet, FIXME", g_switches.size());
    }

    /*
     * Sanity check in metadata make sure that there are no mandatory on create
     * and create only attributes that are obejct id attributes, sinec we would
     * need create those objects first but we need switch first. So here we
     * selecting only MANDATORY_ON_CREATE and CREATE_ONLY attributes to create
     * switch.
     */

    for (const auto& s: g_switches)
    {
        std::string strSwitchVid = s.first;
        std::string asicKey = s.second;

        sai_object_id_t switch_vid;

        sai_deserialize_object_id(strSwitchVid, switch_vid);

        if (switch_vid == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_THROW("switch id can't be NULL");
        }

        auto oit = g_oids.find(strSwitchVid);

        if (oit == g_oids.end())
        {
            SWSS_LOG_THROW("failed to find VID %s in OIDs map", strSwitchVid.c_str());
        }

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        /*
         * If any of those attributes are pointers, fix them, so they will
         * point to callbacks in syncd memory.
         */

        check_notifications_pointers(attrCount, attrList);

        /*
         * Now we need to select only attributes MANDATORY_ON_CREATE and
         * CREATE_ONLY and which will not contain object ids.
         *
         * No need to call processAttributesForOids since we know that there
         * are no OID attributes.
         */

        uint32_t attr_count = 0;            // attr count needed for create
        uint32_t attr_count_left = 0;       // attr count after create

        std::vector<sai_attribute_t> attrs;         // attrs for create
        std::vector<sai_attribute_t> attrs_left;    // attrs for set

        for (uint32_t idx = 0; idx < attrCount; ++idx)
        {
            auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attrList[idx].id);

            if (HAS_FLAG_MANDATORY_ON_CREATE(meta->flags) || HAS_FLAG_CREATE_ONLY(meta->flags))
            {
                /*
                 * If attribute is mandatory on create or create only, we need
                 * to select it for switch create method, since it's required
                 * on create or it will not be possible to change it after
                 * create.
                 */

                attrs.push_back(attrList[idx]); // struct copy, we will keep the same pointers

                attr_count++;
            }
            else
            {
                /*
                 * Those attributes can be OID attributes, so we need to
                 * process them after creating switch.
                 */

                attrs_left.push_back(attrList[idx]); // struct copy, we will keep the same pointers
                attr_count_left++;
            }
        }

        sai_attribute_t *attr_list = attrs.data();

        sai_object_id_t switch_rid;

        sai_status_t status = sai_metadata_sai_switch_api->create_switch(&switch_rid, attr_count, attr_list);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("failed to create switch VID %s: %s",
                    sai_serialize_object_id(switch_rid).c_str(),
                    sai_serialize_status(status).c_str());
        }

        /*
         * Save this switch as translated.
         */

        g_translated[switch_vid] = switch_rid;

        auto sw = switches[switch_vid] = std::make_shared<SaiSwitch>(switch_vid, switch_rid);

        /*
         * We remove default vlan members to make it easier to reinit since our
         * orch agent will remove them anyway.
         *
         * NOTE: To make this fully functional we need make use of method that
         * will list all objects after switch creation.
         */

        sw->removeDefaultVlanMembers();

        /*
         * Since we have only one switch we can get away with this.
         */

        g_switch_rid = switch_rid;
        g_switch_vid = switch_vid;

        g_sw = sw;

        /*
         * We processed switch. We have switch vid/rid so we can process all
         * other attributes of switches that are not mandatory on create and are
         * not crate only.
         *
         * Since those left attributes may contain VIDs we need to process
         * attributes for oids.
         */

        processAttributesForOids(SAI_OBJECT_TYPE_SWITCH, attr_count_left, attrs_left.data());

        for (uint32_t idx = 0; idx < attr_count_left; ++idx)
        {
            sai_attribute_t *attr = &attrs_left[idx];

            status = sai_metadata_sai_switch_api->set_switch_attribute(switch_rid, attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_THROW("failed to set attribute %s on switch VID %s: %s",
                        sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr->id)->attridname,
                        sai_serialize_object_id(switch_rid).c_str(),
                        sai_serialize_status(status).c_str());
            }
        }
    }
}

void trapGroupWorkaround(
        _In_ sai_object_id_t vid,
        _Inout_ sai_object_id_t& rid,
        _In_ bool& createObject,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    if (createObject)
    {
        /*
         * There is a bug on brcm that create trap group with queue attribute
         * will fail, but it can be set after create without this attribute, so
         * we will here create tap group and set it later.
         *
         * This needs to be fixed by brcm.
         */

        createObject = false; // force to "SET" left attributes
    }
    else
    {
        // default trap group
        return;
    }

    sai_object_type_t objectType = SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP;

    SWSS_LOG_INFO("creating trap group and setting attributes 1 by 1 as workaround");

    const sai_attribute_t* queue_attr = sai_metadata_get_attr_by_id(SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE, attrCount, attrList);

    if (queue_attr == NULL)
    {
        SWSS_LOG_THROW("missing QUEUE attribute on TRAP_GROUP creation even if it's not MANDATORY");
    }

    sai_status_t status = sai_metadata_sai_hostif_api->create_hostif_trap_group(&rid, g_switch_rid, 1, queue_attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to create TRAP_GROUP %s",
                sai_serialize_status(status).c_str());
    }

    SWSS_LOG_DEBUG("created TRAP_GROUP (%s), processed VID %s to RID %s",
            sai_serialize_object_type(objectType).c_str(),
            sai_serialize_object_id(vid).c_str(),
            sai_serialize_object_id(rid).c_str());
}

void listFailedAttributes(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < attrCount; idx++)
    {
        const sai_attribute_t *attr = &attrList[idx];

        auto meta = sai_metadata_get_attr_metadata(objectType, attr->id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("failed to get atribute metadata %s %d",
                    sai_serialize_object_type(objectType).c_str(),
                    attr->id);

            continue;
        }

        SWSS_LOG_ERROR("%s = %s", meta->attridname, sai_serialize_attr_value(*meta, *attr).c_str());
    }
}

sai_object_id_t processSingleVid(
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    if (vid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_DEBUG("processed VID 0 to RID 0");

        return SAI_NULL_OBJECT_ID;
    }

    auto it = g_translated.find(vid);

    if (it != g_translated.end())
    {
        /*
         * This object was already processed, just return real object id.
         */

        SWSS_LOG_DEBUG("processed VID %s to RID %s",
                sai_serialize_object_id(vid).c_str(),
                sai_serialize_object_id(it->second).c_str());

        return it->second;
    }

    sai_object_type_t objectType = getObjectTypeFromVid(vid);

    std::string strVid = sai_serialize_object_id(vid);

    auto oit = g_oids.find(strVid);

    if (oit == g_oids.end())
    {
        SWSS_LOG_THROW("failed to find VID %s in OIDs map", strVid.c_str());
    }

    std::string asicKey = oit->second;;

    std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

    sai_attribute_t *attrList = list->get_attr_list();

    uint32_t attrCount = list->get_attr_count();

    processAttributesForOids(objectType, attrCount, attrList);

    bool createObject = true;

    /*
     * Now let's determine whether this object need to be created.  Default
     * obejcts like default virtual router, queues or cpu can't be created.
     *
     * TODO: Should we include vlan members here ?
     *
     * NOTE: this also should be per switch.
     */

    auto vrmapit = g_vidToRidMap.find(vid);

    if (vrmapit == g_vidToRidMap.end())
    {
        SWSS_LOG_THROW("failed to find VID %s in VIDTORID map",
                sai_serialize_object_id(vid).c_str());
    }

    sai_object_id_t rid;

    if (g_sw->isNonCreateRid(vrmapit->second))
    {
        rid = vrmapit->second;

        createObject = false;

        SWSS_LOG_INFO("object %s will not be created, processed VID %s to RID %s",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(vid).c_str(),
                sai_serialize_object_id(rid).c_str());
    }

    if (objectType == SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP)
    {
        /*
         * We need special case for trap group, look inside for details.
         */

        trapGroupWorkaround(vid, rid, createObject, attrCount, attrList);
    }

    auto info = sai_metadata_all_object_type_infos[objectType];

    if (createObject)
    {
        sai_object_meta_key_t meta_key;

        meta_key.objecttype = objectType;

        /*
         * Since we have only one switch, we can get away using g_switch_rid here.
         */

        sai_status_t status = info->create(&meta_key, g_switch_rid, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(objectType, attrCount, attrList);

            SWSS_LOG_THROW("failed to create object %s: %s",
                    sai_serialize_object_type(objectType).c_str(),
                    sai_serialize_status(status).c_str());
        }

        rid = meta_key.objectkey.key.object_id;

        SWSS_LOG_DEBUG("created object of type %s, processed VID %s to RID %s",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(vid).c_str(),
                sai_serialize_object_id(rid).c_str());
    }
    else
    {
        SWSS_LOG_DEBUG("setting attributes on object of type %x, processed VID 0x%lx to RID 0x%lx", objectType, vid, rid);

        for (uint32_t idx = 0; idx < attrCount; idx++)
        {
            sai_attribute_t *attr = &attrList[idx];

            sai_object_meta_key_t meta_key;

            meta_key.objecttype = objectType;
            meta_key.objectkey.key.object_id = rid;

            sai_status_t status = info->set(&meta_key, attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                auto meta = sai_metadata_get_attr_metadata(objectType, attr->id);

                if (meta == NULL)
                {
                    SWSS_LOG_THROW("failed to get atribute metadata %s: %d",
                            sai_serialize_object_type(objectType).c_str(),
                            attr->id);
                }

                SWSS_LOG_THROW(
                        "failed to set %s value %s: %s",
                        meta->attridname,
                        sai_serialize_attr_value(*meta, *attr).c_str(),
                        sai_serialize_status(status).c_str());
            }
        }
    }

    g_translated[vid] = rid;

    return rid;
}

void processAttributesForOids(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("processing list for object type %s",
            sai_serialize_object_type(objectType).c_str());

    for (uint32_t idx = 0; idx < attr_count; idx++)
    {
        sai_attribute_t &attr = attr_list[idx];

        auto meta = sai_metadata_get_attr_metadata(objectType, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(objectType).c_str(),
                    attr.id);
        }

        uint32_t count = 0;
        sai_object_id_t *objectIdList;

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                count = 1;
                objectIdList = &attr.value.oid;
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                count = attr.value.objlist.count;
                objectIdList = attr.value.objlist.list;
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                if (attr.value.aclfield.enable)
                {
                    count = 1;
                    objectIdList = &attr.value.aclfield.data.oid;
                }
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                {
                    count = attr.value.aclfield.data.objlist.count;
                    objectIdList = attr.value.aclfield.data.objlist.list;
                }
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                if (attr.value.aclaction.enable)
                {
                    count = 1;
                    objectIdList = &attr.value.aclaction.parameter.oid;
                }
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                {
                    count = attr.value.aclaction.parameter.objlist.count;
                    objectIdList = attr.value.aclaction.parameter.objlist.list;
                }
                break;

            default:

                // TODO later isoidattribute
                if (meta->allowedobjecttypeslength > 0)
                {
                    SWSS_LOG_THROW("attribute %s is oid attribute, but not processed, FIXME", meta->attridname);
                }

                /*
                 * This is not oid attribute, we can skip processing.
                 */

                continue;
        }

        /*
         * Attribute contains object id's, they need to be translated some of
         * them could be already translated.
         */

        for (uint32_t j = 0; j < count; j++)
        {
            sai_object_id_t vid = objectIdList[j];

            sai_object_id_t rid = processSingleVid(vid);

            objectIdList[j] = rid;
        }
    }
}

void processOids()
{
    for (const auto &kv: g_oids)
    {
        const std::string &strObjectId = kv.first;

        sai_object_id_t vid;
        sai_deserialize_object_id(strObjectId, vid);

        processSingleVid(vid);
    }
}

void processFdbs()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_fdbs)
    {
        const std::string &strFdbEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_fdb_entry_t fdbEntry;
        sai_deserialize_fdb_entry(strFdbEntry, fdbEntry);

        // we could use metadata generic to process

        fdbEntry.switch_id = processSingleVid(fdbEntry.switch_id);
        fdbEntry.bvid = processSingleVid(fdbEntry.bvid);

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_FDB_ENTRY, attrCount, attrList);

        sai_status_t status = sai_metadata_sai_fdb_api->create_fdb_entry(&fdbEntry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(SAI_OBJECT_TYPE_FDB_ENTRY, attrCount, attrList);

            SWSS_LOG_THROW("failed to create_fdb_entry %s: %s",
                    strFdbEntry.c_str(),
                    sai_serialize_status(status).c_str());
        }
    }
}

void processNeighbors()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_neighbors)
    {
        const std::string &strNeighborEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_neighbor_entry_t neighborEntry;
        sai_deserialize_neighbor_entry(strNeighborEntry, neighborEntry);

        // we could use metadata generic to process

        neighborEntry.switch_id = processSingleVid(neighborEntry.switch_id);
        neighborEntry.rif_id = processSingleVid(neighborEntry.rif_id);

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, attrCount, attrList);

        sai_status_t status = sai_metadata_sai_neighbor_api->create_neighbor_entry(&neighborEntry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, attrCount, attrList);

            SWSS_LOG_THROW("failed to create_neighbor_entry %s: %s",
                    strNeighborEntry.c_str(),
                    sai_serialize_status(status).c_str());
        }
    }
}

void processRoutes(bool defaultOnly)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("apply routes");

    for (auto &kv: g_routes)
    {
        const std::string &strRouteEntry = kv.first;
        const std::string &asicKey = kv.second;

        bool isDefault = strRouteEntry.find("/0") != std::string::npos;

        if (defaultOnly ^ isDefault)
        {
            /*
             * Since there is a requirement in brcm that default route needs to
             * be put first in the asic, then we execute default routes first
             * and then other routes.
             */

            continue;
        }

        sai_route_entry_t routeEntry;
        sai_deserialize_route_entry(strRouteEntry, routeEntry);

        // we could use metadata generic to process

        routeEntry.switch_id = processSingleVid(routeEntry.switch_id);
        routeEntry.vr_id = processSingleVid(routeEntry.vr_id);

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_ROUTE_ENTRY, attrCount, attrList);

        sai_status_t status = sai_metadata_sai_route_api->create_route_entry(&routeEntry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(SAI_OBJECT_TYPE_ROUTE_ENTRY, attrCount, attrList);

            SWSS_LOG_THROW(
                    "failed to create ROUTE %s: %s",
                    strRouteEntry.c_str(),
                    sai_serialize_status(status).c_str());
        }
    }
}

std::vector<std::string> redisGetAsicStateKeys()
{
    SWSS_LOG_ENTER();

    return g_redisClient->keys(ASIC_STATE_TABLE + std::string(":*"));
}

void hardReinit()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("hard reinit");

    /*
     * Repopulate asic view from redis db after hard asic initialize.
     */

    /*
     * To support multiple switchies this needs to be refactores since we need
     * a class with hard reinit, per switch
     */

    g_vidToRidMap = redisGetVidToRidMap();
    g_ridToVidMap = redisGetRidToVidMap();

    std::vector<std::string> asicStateKeys = redisGetAsicStateKeys();

    for (const auto &key: asicStateKeys)
    {
        /*
         * TODO if key will be meta_key anyway we could use deserialize here.
         */

        sai_object_type_t objectType = getObjectTypeFromAsicKey(key);
        const std::string &strObjectId = getObjectIdFromAsicKey(key);

        auto info = sai_metadata_all_object_type_infos[objectType];

        switch (objectType)
        {
            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                g_routes[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_FDB_ENTRY:
                g_fdbs[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                g_neighbors[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_SWITCH:
                g_switches[strObjectId] = key;
                break;

            default:

                if (info->isnonobjectid)
                {
                    SWSS_LOG_THROW("passing non object id %s as generic object", info->objecttypename);
                }

                g_oids[strObjectId] = key;
                break;
        }

        g_attributesLists[key] = redisGetAttributesFromAsicKey(key);
    }

    processSwitches();
    processFdbs();
    processNeighbors();
    processOids();
    processRoutes(true);
    processRoutes(false);

    checkAllIds();
}
