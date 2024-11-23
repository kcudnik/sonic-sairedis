#include "VirtualSwitchSaiInterface.h"

#include "swss/logger.h"

#include "vppxlate/SaiIntfStats.h"

using namespace saivs;

// VPP

bool VirtualSwitchSaiInterface::port_to_hostif_list(
        _In_ sai_object_id_t port_id,
        _Inout_ std::string& if_name)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id = switchIdQuery(port_id);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        return false;
    }

    auto it = m_switchStateMap.find(switch_id);

    if (it == m_switchStateMap.end())
    {
        return false;
    }

    auto sw = it->second;

    if (sw == nullptr)
    {
        return false;
    }

    return sw->getTapNameFromPortId(port_id, if_name);
}

bool VirtualSwitchSaiInterface::port_to_hwifname(
        _In_ sai_object_id_t port_id,
        _Inout_ std::string& if_name)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id = switchIdQuery(port_id);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        return false;
    }

    auto it = m_switchStateMap.find(switch_id);

    if (it == m_switchStateMap.end())
    {
        return false;
    }

    auto sw = it->second;

    if (sw == nullptr)
    {
        return false;
    }

    return sw->vpp_get_hwif_name(port_id, 0, if_name);
}

void VirtualSwitchSaiInterface::setPortStats(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    std::map<sai_stat_id_t, uint64_t> stats;

    std::string if_name;

    if (port_to_hwifname(oid, if_name) == false)
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

