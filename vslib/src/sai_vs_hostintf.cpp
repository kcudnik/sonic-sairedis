#include "sai_vs.h"
#include "sai_vs_internal.h"

#include "meta/saiserialize.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <unistd.h>

#include <pcap.h>

// TODO on hostif remove we should stop threads

typedef struct _hostif_info_t
{
    pcap_t *veth_r;
    pcap_t *veth_w;

    int tapfd;

    std::shared_ptr<std::thread> e2t;
    std::shared_ptr<std::thread> t2e;

    sai_object_id_t hostif_vid;

    volatile bool run_thread;

} hostif_info_t;

std::map<std::string, std::shared_ptr<hostif_info_t>> hostif_info_map;

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

int vs_set_dev_mac_address(const char *dev, const sai_mac_t mac)
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
    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0)
    {
        SWSS_LOG_ERROR("failed to open socket: %d", s);

        return -1;
    }

    struct ifreq ifr;

    memset(&ifr, 0, sizeof ifr);

    strncpy(ifr.ifr_name, dev , IFNAMSIZ);

    ifr.ifr_flags |= IFF_UP;

    int err = ioctl(s, SIOCSIFFLAGS, &ifr);

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

    while (info->run_thread)
    {
        struct pcap_pkthdr header;

        unsigned const char *packet = pcap_next(info->veth_r, &header);

        if (packet == NULL)
        {
            continue;
        }

        // TODO examine packet for mac and possible generate fdb_event

        ssize_t wr = write(info->tapfd, packet, header.len);

        if (wr < 0)
        {
            SWSS_LOG_ERROR("failed to write to tapfd: %d", info->tapfd);
        }
    }

    pcap_close(info->veth_r);
}

void tap2veth_fun(std::shared_ptr<hostif_info_t> info)
{
    SWSS_LOG_ENTER();

    unsigned char buffer[0x4000];

    while (info->run_thread)
    {
        ssize_t nread = read(info->tapfd, buffer, sizeof(buffer));

        if (nread < 0)
        {
            SWSS_LOG_ERROR("failed to read from tapfd: %d", info->tapfd);

            break;
        }

        int ret = pcap_sendpacket(info->veth_w, buffer, (int)nread);

        if (ret < 0)
        {
            SWSS_LOG_ERROR("failed to send packet via pcap from tapfd: %d", info->tapfd);
        }
    }

    pcap_close(info->veth_w);
}

bool hostif_create_tap_veth_forwarding(
        _In_ const std::string &tapname,
        _In_ int tapfd)
{
    SWSS_LOG_ENTER();

    // we assume here that veth devices were added by user before creating this
    // host interface, vEthernetX will be used for packet transfer between ip
    // namespaces

    std::string vethname = "v" + tapname;

    char errbuf[PCAP_ERRBUF_SIZE];

    pcap_t *veth_r = pcap_open_live(vethname.c_str(), BUFSIZ, 0, 300, errbuf);

    // open veth device for read and for write (we need 2 descriptiors since
    // this is not thread safe)

    if (veth_r == NULL)
    {
        SWSS_LOG_ERROR("Couldn't open device %s: %s", vethname.c_str(), errbuf);

        return false;
    }

    pcap_t *veth_w = pcap_open_live(vethname.c_str(), BUFSIZ, 0, 300, errbuf);

    if (veth_w == NULL)
    {
        SWSS_LOG_ERROR("Couldn't open device %s: %s", vethname.c_str(), errbuf);

        pcap_close(veth_r);

        return false;
    }

    std::shared_ptr<hostif_info_t> info;

    hostif_info_map[tapname] = info;

    info->veth_r     = veth_r;
    info->veth_w     = veth_w;
    info->tapfd      = tapfd;
    info->run_thread = true;
    info->e2t        = std::make_shared<std::thread>(veth2tap_fun, info);
    info->t2e        = std::make_shared<std::thread>(tap2veth_fun, info);

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
        return SAI_STATUS_SUCCESS;
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

    if (strncmp(attr_name->value.chardata, "Ethernet", 8) != 0)
    {
        SWSS_LOG_ERROR("interface name should start with EthernetX but is %s", attr_name->value.chardata);

        return SAI_STATUS_FAILURE;
    }

    std::string name = std::string(attr_name->value.chardata);

    // create TAP device

    SWSS_LOG_INFO("creating hostif %s", name.c_str());

    int tapfd = vs_create_tap_device(name.c_str(), IFF_TAP);

    if (tapfd < 0)
    {
        SWSS_LOG_ERROR("failed to create TAP device for %s", name.c_str());

        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_INFO("created TAP device for %s, fd: %d", name.c_str(), tapfd);

    // XXX currently tapfd is ignored, it should be closed on hostif_remove
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

    err = ifup(name.c_str());

    if (err < 0)
    {
        SWSS_LOG_ERROR("failed to bring ifup %s", name.c_str());

        close(tapfd);

        return SAI_STATUS_FAILURE;
    }

    if (!hostif_create_tap_veth_forwarding(name, tapfd))
    {
        SWSS_LOG_ERROR("forwarding rule on %s was not added", name.c_str());
    }

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
