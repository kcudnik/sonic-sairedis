#include "sai_vs.h"
#include "sai_vs_internal.h"
#include "sai_vs_state.h"

#include "meta/saiserialize.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <net/if_arp.h>
#include <linux/if_ether.h>

// TODO on hostif remove we should stop threads

typedef struct _hostif_info_t
{
    int tapfd;
    int packet_socket;

    std::shared_ptr<std::thread> e2t;
    std::shared_ptr<std::thread> t2e;

    sai_object_id_t hostif_vid;

    volatile bool run_thread;

    std::string name;

    sai_object_id_t portid;

} hostif_info_t;

std::map<std::string, std::shared_ptr<hostif_info_t>> hostif_info_map;

std::set<fdb_info_t> g_g_fdb_info_set;

void processFdbInfoLearned(
        _In_ fdb_info_t &fi,
        _In_ const std::shared_ptr<hostif_info_t> &info)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    // TODO call user notification from switch NOTIFY
    
    // for looking on bridge port 
    // auto &objectHash = g_switch_state_map.at(switch_id)->objectHash.at(object_type);

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;

    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = 0; // TODO need to get bridge port id

    sai_fdb_event_notification_data_t data;

    data.event_type = SAI_FDB_EVENT_LEARNED;

    data.fdb_entry.switch_id = fi.switchid;
    data.fdb_entry.mac_address = fi.src;
    data.fdb_entry.vlan_id = fi.vlanid;
    data.fdb_entry.bridge_type = 0; // TODO
    data.fdb_entry.bridge_id; // TODO
    
    data.attr_count = 2;
    data.attr_list = &attrs;

    meta_sai_on_fdb_event(1, &data);

    // TODO call user callback
}

void process_packet_for_fdb_event(
        _In_ const uint8_t *buffer,
        _In_ size_t size,
        _In_ const std::shared_ptr<hostif_info_t> &info)
{
    MUTEX();

    SWSS_LOG_ENTER();

    uint32_t frametime = (uint32_t)time(NULL);

    /*
     * We add +2 in case if frame contains 1Q VLAN tag.
     */

    if (size < (sizeof(ethhdr) + 2))
    {
        SWSS_LOG_WARN("ethernet frame is too small: %zu", size);
        return;
    }

    const ethhdr *eh = (const ethhdr*)buffer;

    uint16_t proto = htons(eh->h_proto);

    uint16_t vlanid = 1;

    if (proto == ETH_P_IP)
    {
        // IP frame, we need to get vlan if from port

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;

        sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_PORT, info->portid, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_WARN("failed to get port vlan id from port %s",
                    sai_serialize_object_id(info->portid).c_str());
            return;
        }

        vlanid = attr.value.u16;
    }
    else if (proto == ETH_P_8021Q)
    {
        // this is tagged frame, get vlan id from frame

        uint16_t tci = htons(eh->h_proto);

        vlanid = tci & 0xfff;

        if (vlanid == 0 || vlanid == 0xfff)
        {
            SWSS_LOG_WARN("invalid vlanid %u in ethernet frame on %s", vlanid, info->name.c_str());
            return;
        }
    }
    else
    {
        SWSS_LOG_WARN("unknown ethernet protocol: 0x%x on %s", proto, info->name.c_str());
        return;
    }

    sai_object_id_t switchid = sai_switch_id_query(info->portid);

    if (switchid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_WARN("got NULL switch if for port %s",
                sai_serialize_object_id(info->portid));
        return;
    }

    // source mac + vlan id is a key

    fdb_info_t fi;

    fi.vlanid = vlanid;
    fi.portid = info->portid;
    fi.switchid = switchid;
    fi.timestamp = frametime;

    memcpy(fi.src, eh->h_source, sizeof(sai_mac_t));

    // TODO chck if this key is already present
    // TODO should key be per bridge or global?
    // xxxx
    // TODO bridge port may not exists if it was removed
    // else we need to get bridge port type and bridge_id

    std::set<fdb_info_t>::iterator it = g_fdb_info_set.find(fi);

    if (it == g_fdb_info_set.end())
    {
        // TODO generate fdb notification

        SWSS_LOG_NOTICE("noticed new mac on %s", info->name.c_str());

        processFdbInfoLearned(fi, info);
    }

    // TODO maybe we should not update everything, just timestamp
    // since we may have fdb_entry inside? for convinience
    g_fdb_info_set.insert(fi);
}

#define MAX_INTERFACE_NAME_LEN IFNAMSIZ

sai_status_t vs_recv_hostif_packet(
        _In_ sai_object_id_t hif_id,
        _Out_ void *buffer,
        _Inout_ sai_size_t *buffer_size,
        _Inout_ uint32_t *attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_send_hostif_packet(
        _In_ sai_object_id_t hif_id,
        _In_ void *buffer,
        _In_ sai_size_t buffer_size,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

int vs_create_tap_device(const char *dev, int flags)
{
    SWSS_LOG_ENTER();

    const char *tundev = "/dev/net/tun";

    int fd = open(tundev, O_RDWR);

    if (fd < 0)
    {
        SWSS_LOG_ERROR("failed to open %s", tundev);

        return -1;
    }

    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = (short int)flags;  // IFF_TUN or IFF_TAP, IFF_NO_PI

    strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    int err = ioctl(fd, TUNSETIFF, (void *) &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl TUNSETIFF on fd %d %s failed, err %d", fd, dev, err);

        close(fd);

        return err;
    }

    return fd;
}

int vs_set_dev_mac_address(const char *dev, const sai_mac_t& mac)
{
    SWSS_LOG_ENTER();

    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0)
    {
        SWSS_LOG_ERROR("failed to create socket, errno: %d", errno);

        return -1;
    }

    struct ifreq ifr;

    strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    memcpy(ifr.ifr_hwaddr.sa_data, mac, 6);

    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

    int err = ioctl(s, SIOCSIFHWADDR, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCSIFHWADDR on socket %d %s failed, err %d", s, dev, err);
    }

    close(s);

    return err;
}

int ifup(const char *dev)
{
    SWSS_LOG_ENTER();

    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0)
    {
        SWSS_LOG_ERROR("failed to open socket: %d", s);

        return -1;
    }

    struct ifreq ifr;

    memset(&ifr, 0, sizeof ifr);

    strncpy(ifr.ifr_name, dev , IFNAMSIZ);

    int err = ioctl(s, SIOCGIFFLAGS, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCGIFFLAGS on socket %d %s failed, err %d", s, dev, err);

        close(s);

        return err;
    }

    ifr.ifr_flags |= IFF_UP;

    err = ioctl(s, SIOCSIFFLAGS, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCSIFFLAGS on socket %d %s failed, err %d", s, dev, err);
    }

    close(s);

    return err;
}

void veth2tap_fun(std::shared_ptr<hostif_info_t> info)
{
    SWSS_LOG_ENTER();

    unsigned char buffer[0x4000];

    while (info->run_thread)
    {
        // TODO convert to non blocking using select

        ssize_t size = read(info->packet_socket, buffer, sizeof(buffer));

        if (size < 0)
        {
            SWSS_LOG_ERROR("failed to read from socket fd %d, errno(%d): %s",
                    info->packet_socket, errno, strerror(errno));

            continue;
        }

        process_packet_for_fdb_event(buffer, size, info);

        if (write(info->tapfd, buffer, size) < 0)
        {
            /*
             * We filter out EIO because of this patch:
             * https://github.com/torvalds/linux/commit/1bd4978a88ac2589f3105f599b1d404a312fb7f6
             */

            if (errno != ENETDOWN && errno != EIO)
            {
                SWSS_LOG_ERROR("failed to write to tap device fd %d, errno(%d): %s",
                        info->tapfd, errno, strerror(errno));
            }

            continue;
        }
    }

    SWSS_LOG_NOTICE("ending thread proc for %s", info->name.c_str());
}

void tap2veth_fun(std::shared_ptr<hostif_info_t> info)
{
    SWSS_LOG_ENTER();

    unsigned char buffer[0x4000];

    while (info->run_thread)
    {
        // TODO convert to non blocking using select

        ssize_t size = read(info->tapfd, buffer, sizeof(buffer));

        if (size < 0)
        {
            SWSS_LOG_ERROR("failed to read from tapfd fd %d, errno(%d): %s",
                    info->tapfd, errno, strerror(errno));

            continue;
        }

        if (write(info->packet_socket, buffer, (int)size) < 0)
        {
            SWSS_LOG_ERROR("failed to write to socket fd %d, errno(%d): %s",
                    info->packet_socket, errno, strerror(errno));

            continue;
        }
    }

    SWSS_LOG_NOTICE("ending thread proc for %s", info->name.c_str());
}

bool hostif_create_tap_veth_forwarding(
        _In_ const std::string &tapname,
        _In_ int tapfd,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    // we assume here that veth devices were added by user before creating this
    // host interface, vEthernetX will be used for packet transfer between ip
    // namespaces

    std::string vethname = SAI_VS_VETH_PREFIX + tapname;

    int packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    if (packet_socket < 0)
    {
        SWSS_LOG_ERROR("failed to open packet socket, errno: %d", errno);

        return false;
    }

    // bind to device

    struct sockaddr_ll sock_address;

    memset(&sock_address, 0, sizeof(sock_address));

    sock_address.sll_family = PF_PACKET;
    sock_address.sll_protocol = htons(ETH_P_ALL);
    sock_address.sll_ifindex = if_nametoindex(vethname.c_str());

    if (sock_address.sll_ifindex == 0)
    {
        SWSS_LOG_ERROR("failed to get interface index for %s", vethname.c_str());

        close(packet_socket);

        return false;
    }

    SWSS_LOG_INFO("interface index = %d %s\n", sock_address.sll_ifindex, vethname.c_str());

    if (bind(packet_socket, (struct sockaddr*) &sock_address, sizeof(sock_address)) < 0)
    {
        SWSS_LOG_ERROR("bind failed on %s", vethname.c_str());

        close(packet_socket);

        return false;
    }

    std::shared_ptr<hostif_info_t> info = std::make_shared<hostif_info_t>();

    hostif_info_map[tapname] = info;

    info->packet_socket = packet_socket;
    info->tapfd         = tapfd;
    info->run_thread    = true;
    info->e2t           = std::make_shared<std::thread>(veth2tap_fun, info);
    info->t2e           = std::make_shared<std::thread>(tap2veth_fun, info);
    info->name          = tapname;
    info->portid        = port_id;

    info->e2t->detach();
    info->t2e->detach();

    SWSS_LOG_NOTICE("setup forward rule for %s succeeded", tapname.c_str());

    return true;
}

sai_status_t vs_create_hostif_int(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t *hostif_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (g_vs_hostif_use_tap_device == false)
    {
        return vs_generic_create(object_type,
                hostif_id,
                switch_id,
                attr_count,
                attr_list);
    }

    // validate SAI_HOSTIF_ATTR_TYPE

    auto attr_type = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_TYPE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }

    if (attr_type->value.s32 != SAI_HOSTIF_TYPE_NETDEV)
    {
        SWSS_LOG_ERROR("only SAI_HOSTIF_TYPE_NETDEV is supported");

        return SAI_STATUS_FAILURE;
    }

    // validate SAI_HOSTIF_ATTR_OBJ_ID

    auto attr_obj_id = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_OBJ_ID, attr_count, attr_list);

    if (attr_obj_id == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_OBJ_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t obj_id = attr_obj_id->value.oid;

    sai_object_type_t ot = sai_object_type_query(obj_id);

    if (ot != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("SAI_HOSTIF_ATTR_OBJ_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(obj_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }

    // validate SAI_HOSTIF_ATTR_NAME

    auto attr_name = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_NAME, attr_count, attr_list);

    if (attr_name == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_NAME was not passed");

        return SAI_STATUS_FAILURE;
    }

    if (strnlen(attr_name->value.chardata, sizeof(attr_name->value.chardata)) >= MAX_INTERFACE_NAME_LEN)
    {
        SWSS_LOG_ERROR("interface name is too long: %.*s", MAX_INTERFACE_NAME_LEN, attr_name->value.chardata);

        return SAI_STATUS_FAILURE;
    }

    std::string name = std::string(attr_name->value.chardata);

    // create TAP device

    SWSS_LOG_INFO("creating hostif %s", name.c_str());

    int tapfd = vs_create_tap_device(name.c_str(), IFF_TAP | IFF_MULTI_QUEUE | IFF_NO_PI);

    if (tapfd < 0)
    {
        SWSS_LOG_ERROR("failed to create TAP device for %s", name.c_str());

        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_INFO("created TAP device for %s, fd: %d", name.c_str(), tapfd);

    // TODO currently tapfd is ignored, it should be closed on hostif_remove
    // and we should use it to read/write packets from that interface

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;

    sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get SAI_SWITCH_ATTR_SRC_MAC_ADDRESS on switch %s: %s",
                sai_serialize_object_id(switch_id).c_str(),
                sai_serialize_status(status).c_str());
    }

    int err = vs_set_dev_mac_address(name.c_str(), attr.value.mac);

    if (err < 0)
    {
        SWSS_LOG_ERROR("failed to set MAC address %s for %s",
                sai_serialize_mac(attr.value.mac).c_str(),
                name.c_str());

        close(tapfd);

        return SAI_STATUS_FAILURE;
    }

    if (!hostif_create_tap_veth_forwarding(name, tapfd, obj_id))
    {
        SWSS_LOG_ERROR("forwarding rule on %s was not added", name.c_str());
    }

    std::string vname = SAI_VS_VETH_PREFIX + name;

    SWSS_LOG_INFO("mapping interface %s to port id %s",
            vname.c_str(),
            sai_serialize_object_id(obj_id).c_str());

    g_switch_state_map.at(switch_id)->setIfNameToPortId(vname, obj_id);

    // TODO what about FDB entries notifications, they also should
    // be generated if new mac addres will show up on the interface/arp table

    // TODO IP address should be assigned when router interface is created

    SWSS_LOG_INFO("created tap interface %s", name.c_str());

    return vs_generic_create(object_type,
            hostif_id,
            switch_id,
            attr_count,
            attr_list);
}

sai_status_t vs_create_hostif(
        _Out_ sai_object_id_t *hostif_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_HOSTIF,
            hostif_id,
            switch_id,
            attr_count,
            attr_list,
            &vs_create_hostif_int);
}

// TODO set must also be supported when we change oper status up/down
// and probably also generate notification then

VS_REMOVE(HOSTIF,hostif);
VS_SET(HOSTIF,hostif);
VS_GET(HOSTIF,hostif);

VS_GENERIC_QUAD(HOSTIF_TABLE_ENTRY,hostif_table_entry);
VS_GENERIC_QUAD(HOSTIF_TRAP_GROUP,hostif_trap_group);
VS_GENERIC_QUAD(HOSTIF_TRAP,hostif_trap);
VS_GENERIC_QUAD(HOSTIF_USER_DEFINED_TRAP,hostif_user_defined_trap);

const sai_hostif_api_t vs_hostif_api = {

    VS_GENERIC_QUAD_API(hostif)
    VS_GENERIC_QUAD_API(hostif_table_entry)
    VS_GENERIC_QUAD_API(hostif_trap_group)
    VS_GENERIC_QUAD_API(hostif_trap)
    VS_GENERIC_QUAD_API(hostif_user_defined_trap)

    vs_recv_hostif_packet,
    vs_send_hostif_packet,
};
