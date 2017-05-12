#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern "C" {
#include <sai.h>
#include "saimetadata.h"
}

#include "sairedis.h"
#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"
#include "swss/redisclient.h"
#include "swss/dbconnector.h"
#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/notificationconsumer.h"
#include "swss/notificationproducer.h"
#include "swss/selectableevent.h"
#include "swss/select.h"
#include "swss/logger.h"
#include "swss/table.h"

#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <chrono>

/*
 * We want to find all object present on the switch
 *
 * Depends on the attribute flags if not read only
 * we can build asic view, and mark some attributes
 * as removable/nonremovable
 */

#define MAX_ELEMENTS 0x10000

int callCount = 0;

std::set<sai_object_id_t> processed;

typedef std::chrono::duration<double, std::ratio<1>> second_t;

size_t discover(
        _In_ sai_object_id_t id)
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: This method is only good after switch init
     * since we are making assumptions that tere are no
     * ACL after initialization.
     */

    if (id == SAI_NULL_OBJECT_ID)
    {
        return processed.size();
    }

    if (processed.find(id) != processed.end())
    {
        return processed.size();
    }

    SWSS_LOG_INFO("processing 0x%lx: %s", id,
            sai_serialize_object_type(sai_object_type_query(id)).c_str());

    processed.insert(id);

    sai_object_type_t ot = sai_object_type_query(id);

    if (ot == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_ERROR("id 0x%lx returned NULL object type", id);
        return processed.size();
    }

    const sai_object_type_info_t *info =  sai_all_object_type_infos[ot]; // sai_metadata_get_object_type_info(ot);

    /*
     * Since we know that we will be quering only oid object types
     * then we dont need meta key, but we need to add to metadata
     * pointers to only generic functions.
     */

    sai_object_meta_key_t mk = { .objecttype = ot, .objectkey = { .key = { .object_id = id } } };

    // for each attribute
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
                 *
                 * But vendor could change or assign its value.
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

            SWSS_LOG_INFO("getting %s for 0x%lx", md->attridname, id);

            sai_status_t status = info->get(&mk, 1, &attr);

            callCount++;

            if (status != SAI_STATUS_SUCCESS)
            {
                /*
                 * We failed to get value, maybe it's not supported ?
                 */

                SWSS_LOG_WARN("%s: %s",
                        md->attridname,
                        sai_serialize_status(status).c_str());

                continue;
            }

            discover(attr.value.oid); // recursion
        }
        else if (md->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_LIST)
        {
            if (md->defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST)
            {
                /*
                 * This means that default value for this object is
                 * empty list, since this is discovery after
                 * create, we don't need to query this attribute.
                 *
                 * But vendor could change or assign its value.
                 */

                continue;
            }

            SWSS_LOG_INFO("getting %s for 0x%lx", md->attridname, id);

            attr.value.objlist.count = MAX_ELEMENTS;
            attr.value.objlist.list = (sai_object_id_t*)alloca(sizeof(sai_object_id_t) * MAX_ELEMENTS);

            sai_status_t status = info->get(&mk, 1, &attr);

            callCount++;

            if (status != SAI_STATUS_SUCCESS)
            {
                /*
                 * We failed to get value, maybe it's not supported ?
                 */

                SWSS_LOG_WARN("%s: %s",
                        md->attridname,
                        sai_serialize_status(status).c_str());

                continue;
            }

            SWSS_LOG_INFO("list count %s %u", md->attridname, attr.value.objlist.count);

            for (uint32_t i = 0; i < attr.value.objlist.count; ++i)
            {
                discover(attr.value.objlist.list[i]); // recursion
            }
        }
    }

    return processed.size();
}

std::map<std::string, std::string> gProfileMap = {
    { "SAI_INIT_CONFIG_FILE", "/usr/share/sai_2700.xml" },
    { "DEVICE_MAC_ADDRESS", "7c:fe:90:5e:6a:80" }
};

std::map<std::string, std::string>::iterator gProfileIter = gProfileMap.begin();

const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return NULL;
    }

    auto it = gProfileMap.find(variable);

    if (it == gProfileMap.end())
    {
        SWSS_LOG_NOTICE("%s: NULL", variable);
        return NULL;
    }

    SWSS_LOG_NOTICE("%s: %s", variable, it->second.c_str());

    return it->second.c_str();
}

int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        gProfileIter = gProfileMap.begin();
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    if (gProfileIter == gProfileMap.end())
    {
        SWSS_LOG_INFO("iterator reached end");
        return -1;
    }

    *variable = gProfileIter->first.c_str();
    *value = gProfileIter->second.c_str();

    SWSS_LOG_INFO("key: %s:%s", *variable, *value);

    gProfileIter++;

    return 0;
}

service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    bool debug = false;
    bool init = true;

    for (int i = 0 ; i < argc; ++i)
    {
        if (strcmp(argv[i], "-I") == 0)
        {
            init = false;
        }

        if (strcmp(argv[i], "-d") == 0)
        {
            debug = true;
        }
    }

    if (!debug)
    {
        swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);
    }

    sai_status_t status = sai_api_initialize(0, (service_method_table_t*)&test_services);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("fail to sai_api_initialize: %d", status);
        exit(EXIT_FAILURE);
    }

    sai_meta_apis_query(sai_api_query);

    sai_object_id_t switch_rid;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = init;

    // TODO in the future there may be more mandatory attributes when creating switch

    status = sai_metadata_sai_switch_api->create_switch(&switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to create a switch: %s", sai_serialize_status(status).c_str());
        exit(EXIT_FAILURE);
    }

    if (sai_object_type_query(switch_rid) != SAI_OBJECT_TYPE_SWITCH)
    {
        SWSS_LOG_ERROR("create switch returned invalid oid: 0x%lx", switch_rid);
        exit(EXIT_FAILURE);
    }

    auto m_start = std::chrono::high_resolution_clock::now();

    size_t count = discover(switch_rid); // ~ 0.02 sec, ~ 3483 calls

    auto end = std::chrono::high_resolution_clock::now();

    double duration = std::chrono::duration_cast<second_t>(end - m_start).count();

    SWSS_LOG_NOTICE("done, processed obejcts: %zu took %lf, call count: %d", count, duration, callCount);

    std::map<sai_object_type_t,int> map;

    for (const auto &p: processed)
    {
        map[sai_object_type_query(p)]++;
    }

    for (const auto &p: map)
    {
        SWSS_LOG_NOTICE("%s: %d", sai_serialize_object_type(p.first).c_str(), p.second);
    }
}
