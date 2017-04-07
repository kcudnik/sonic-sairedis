#ifndef __SAI_VS_STATE__
#define __SAI_VS_STATE__

#include "meta/sai_meta.h"
#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"

#include <unordered_map>
#include <string>
#include <set>

#define CHECK_STATUS(status) \
{\
    sai_status_t s = (status);\
    if (s != SAI_STATUS_SUCCESS) \
        { return s; }\
}

typedef std::unordered_map<std::string, std::string> AttrHash;
typedef std::unordered_map<std::string, AttrHash> ObjectHash;

#define DEFAULT_VLAN_NUMBER 1

class SwitchState
{
    public:

        SwitchState(
                _In_ sai_object_id_t switch_id):
            m_switch_id(switch_id)
        {

            if (sai_object_type_query(switch_id) != SAI_OBJECT_TYPE_SWITCH)
            {
                SWSS_LOG_THROW("object %s is not SWITCH, its %s",
                        sai_serialize_object_id(switch_id).c_str(),
                        sai_serialize_object_type(sai_object_type_query(switch_id)).c_str());
            }

            // create switch by default
            // (it will require special treat on creating

            objectHash[sai_serialize_object_id(switch_id)] = {};
        }

    ObjectHash objectHash;

    sai_object_id_t default_vlan_id;
    sai_object_id_t default_1q_bridge;

    std::map<sai_object_id_t, std::set<sai_object_id_t>> vlan_members_map;

    sai_object_id_t getSwitchId() const
    {
        return m_switch_id;
    }

    private:

        sai_object_id_t m_switch_id;
};

void update_vlan_member_list_on_vlan(
        _In_ sai_object_id_t vlan_id);

typedef std::map<sai_object_id_t, std::shared_ptr<SwitchState>> SwitchStateMap;

extern SwitchStateMap g_switch_state_map;

void vs_reset_id_counter();
void vs_clear_switch_ids();
void vs_free_real_object_id(
        _In_ sai_object_id_t switch_id);

sai_object_id_t vs_create_real_object_id(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id);

#endif // __SAI_VS_STATE__
