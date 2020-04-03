extern "C" {
#include "sai.h"
}

#include "sairedis.h"

#include "swss/logger.h"
#include "swss/warm_restart.h"

#include <unistd.h>

#include <vector>
#include <fstream>
#include <map>
#include <string>

static sai_switch_api_t* switch_api;

sai_object_id_t create_switch_on_syncd_instance(
    _In_ uint32_t context)
{
    SWSS_LOG_ENTER();

    std::vector<sai_attribute_t> attrs;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    attrs.push_back(attr);

    // different switches on same syncd instances must have different hardware
    // info attribute

    std::string hwinfo ="";

    attr.id = SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO;
    attr.value.s8list.count = hwinfo.size();
    attr.value.s8list.list = NULL; // (int8_t*)hwinfo.c_str();

    attrs.push_back(attr);

    // ... more attributes

    // last attribute must be context (syncd instance)

    attr.id = SAI_REDIS_SWITCH_ATTR_CONTEXT;
    attr.value.u32 = context; // NOTICE

    attrs.push_back(attr);

    sai_object_id_t switchId;

    SWSS_LOG_NOTICE("attempt to create switch at context %u", context);

    sai_status_t status = switch_api->create_switch(&switchId, (uint32_t)attrs.size(), attrs.data());

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to create a switch, rv:%d", status);
        exit(EXIT_FAILURE);
    }

    return switchId;
}

static std::map<std::string, std::string> gProfileMap;

const char *test_profile_get_value (
    _In_ sai_switch_profile_id_t profile_id,
    _In_ const char *variable)
{
    SWSS_LOG_ENTER();

    auto it = gProfileMap.find(variable);

    if (it == gProfileMap.end())
        return NULL;
    return it->second.c_str();
}

int test_profile_get_next_value (
    _In_ sai_switch_profile_id_t profile_id,
    _Out_ const char **variable,
    _Out_ const char **value)
{
    SWSS_LOG_ENTER();

    static auto it = gProfileMap.begin();

    if (value == NULL)
    {
        // Restarts enumeration
        it = gProfileMap.begin();
    }
    else if (it == gProfileMap.end())
    {
        return -1;
    }
    else
    {
        *variable = it->first.c_str();
        *value = it->second.c_str();
        it++;
    }

    if (it != gProfileMap.end())
        return 0;
    else
        return -1;
}

const sai_service_method_table_t test_services = {
    test_profile_get_value,
    test_profile_get_next_value
};

void loadProfileMap(const std::string& profileMapFile)
{
    SWSS_LOG_ENTER();

    std::ifstream profile(profileMapFile);

    if (!profile.is_open())
    {
        SWSS_LOG_ERROR("failed to open profile map file: %s: %s",
                profileMapFile.c_str(),
                strerror(errno));

        exit(EXIT_FAILURE);
    }

    std::string line;

    while (getline(profile, line))
    {
        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        size_t pos = line.find("=");

        if (pos == std::string::npos)
        {
            SWSS_LOG_WARN("not found '=' in line %s", line.c_str());
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        gProfileMap[key] = value;

        SWSS_LOG_INFO("insert: %s:%s", key.c_str(), value.c_str());
    }
}

sai_object_id_t getVr(
        _In_ sai_object_id_t switchId)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    // Get the default virtual router ID
    attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;

    sai_status_t status = switch_api->get_switch_attribute(switchId, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Fail to get switch virtual router ID %d", status);
        exit(EXIT_FAILURE);
    }

    return attr.value.oid;
}

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    swss::Logger::linkToDbNative("test");

    swss::WarmStart::initialize("test", "swss");
    swss::WarmStart::checkWarmStart("test", "swss");

    loadProfileMap("vsprofile.ini"); // need for SAI_REDIS_CONTEXT_CONFIG

    sai_api_initialize(0, (const sai_service_method_table_t *)&test_services);

    sai_api_query(SAI_API_SWITCH, (void **)&switch_api);

    auto switch0 = create_switch_on_syncd_instance(0);
    auto switch1 = create_switch_on_syncd_instance(1);

    SWSS_LOG_NOTICE("success creting switch");

    getVr(switch0);
    getVr(switch1);

    SWSS_LOG_NOTICE("got VR success");

    // perform other SAI actions
    
    sleep(10000);

    return 0;
}
