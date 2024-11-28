#include "SwitchVpp.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include "vppxlate/SaiIntfStats.h"

using namespace saivs;

// TODO init vpp

SwitchVpp::SwitchVpp(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchStateBase(switch_id, manager, config),
    m_object_db(this),
    m_tunnel_mgr(this)
{
    SWSS_LOG_ENTER();

    // empty
}

SwitchVpp::SwitchVpp(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config,
        _In_ std::shared_ptr<WarmBootState> warmBootState):
    SwitchStateBase(switch_id, manager, config, warmBootState),
    m_object_db(this),
    m_tunnel_mgr(this)
{
    SWSS_LOG_ENTER();

    // empty
}

bool SwitchVpp::port_to_hostif_list(
        _In_ sai_object_id_t port_id, 
        _Inout_ std::string& if_name)
{
    SWSS_LOG_ENTER();

    // TODO to be removed and inlined

    //sai_object_id_t switch_id = switchIdQuery(port_id);
    //if (switch_id == SAI_NULL_OBJECT_ID) {
    //return false;
    //}
    //auto it = m_switchStateMap.find(switch_id);
    //if (it == m_switchStateMap.end()) {
    //return false;
    //}
    //auto sw = it->second;
    //if (sw == nullptr) {
    //return false;
    //}
    //return(
    return getTapNameFromPortId(port_id, if_name);
}

bool SwitchVpp::port_to_hwifname(
        _In_ sai_object_id_t port_id,
        _Inout_ std::string& if_name)
{
    SWSS_LOG_ENTER();

    // TODO to be removed and inlined

    // sai_object_id_t switch_id = switchIdQuery(port_id);
    // if (switch_id == SAI_NULL_OBJECT_ID) {
    // return false;
    // }
    // auto it = m_switchStateMap.find(switch_id);
    // if (it == m_switchStateMap.end()) {
    // return false;
    // }
    // auto sw = it->second;
    // if (sw == nullptr) {
    // return false;
    // }

    return vpp_get_hwif_name(port_id, 0, if_name);
}

void SwitchVpp::setPortStats(
        _In_ sai_object_id_t oid)
{
    std::map<sai_stat_id_t, uint64_t> stats;

    std::string if_name;

    if (!port_to_hwifname(oid, if_name))
    {
        return;
    }

    vpp_interface_stats_t port_stats;

    if (vpp_intf_stats_query(if_name.c_str(), &port_stats) == 0)
    {
        stats[SAI_PORT_STAT_IF_IN_OCTETS] = port_stats.rx_bytes;
        stats[SAI_PORT_STAT_IF_IN_UCAST_PKTS] = port_stats.rx;
        stats[SAI_PORT_STAT_IF_IN_BROADCAST_PKTS] = port_stats.rx_broadcast;
        stats[SAI_PORT_STAT_IF_IN_MULTICAST_PKTS] = port_stats.rx_multicast;
        stats[SAI_PORT_STAT_IF_IN_DISCARDS] = port_stats.drops;
        stats[SAI_PORT_STAT_IF_OUT_OCTETS] = port_stats.tx_bytes;
        stats[SAI_PORT_STAT_IF_OUT_UCAST_PKTS] = port_stats.tx;
        stats[SAI_PORT_STAT_IF_OUT_BROADCAST_PKTS] = port_stats.tx_broadcast;
        stats[SAI_PORT_STAT_IF_OUT_MULTICAST_PKTS] = port_stats.tx_multicast;

        stats[SAI_PORT_STAT_IN_DROPPED_PKTS] = port_stats.rx_no_buf;
        stats[SAI_PORT_STAT_IF_IN_ERRORS] = port_stats.rx_error;
        stats[SAI_PORT_STAT_IF_OUT_ERRORS] = port_stats.tx_error;
        stats[SAI_PORT_STAT_IP_IN_RECEIVES] = port_stats.ip4;
        stats[SAI_PORT_STAT_IPV6_IN_RECEIVES] = port_stats.ip6;
    }

    debugSetStats(oid, stats);
}

sai_status_t SwitchVpp::queryAttributeCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Out_ sai_attr_capability_t *capability)
{
    SWSS_LOG_ENTER();

    // TODO: We should generate this metadata for the virtual switch rather
    // than hard-coding it here.

    // in virtual switch by default all apis are implemented for all objects. SUCCESS for all attributes

    capability->create_implemented = true;
    capability->set_implemented    = true;
    capability->get_implemented    = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_PORT)
    {
        setPortStats(object_id);
    }

    return SwitchStateBase::getStatsExt(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

void SwitchVpp::processFdbEntriesForAging()
{
    SWSS_LOG_ENTER();

    return;
}
