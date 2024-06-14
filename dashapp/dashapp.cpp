#include "swss/logger.h"

#include "meta/sai_serialize.h"
#include "Proxy.h"
#include "VendorSai.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <math.h>

#include <thread>

typedef struct _tap_info_t
{
    int tapfd;
    int packet_socket;

    std::shared_ptr<std::thread> e2t;
    std::shared_ptr<std::thread> t2e;

    volatile bool run_thread;

    std::string name;

} tap_info_t;


static void process_packet_for_fdb_event(
        const uint8_t *buffer,
        size_t size,
        std::shared_ptr<tap_info_t> info)
{
    SWSS_LOG_ENTER();

    //We add +4 in case if frame contains 1Q VLAN tag.

    if (size < (sizeof(ethhdr) + 4))
    {
        SWSS_LOG_ERROR("ethernet frame is too small: %zu", size);
        return;
    }

    const ethhdr *eh = (const ethhdr*)buffer;

    uint16_t proto = htons(eh->h_proto);

    SWSS_LOG_NOTICE("proto = 0x%x", proto);

    uint16_t vlan_id = 1;

    bool tagged = (proto == ETH_P_8021Q);

    if (tagged)
    {
        // this is tagged frame, get vlan id from frame

        uint16_t tci = htons(((const uint16_t*)&eh->h_proto)[1]); // tag is after h_proto field

        vlan_id = tci & 0xfff;

        if (vlan_id == 0xfff)
        {
            SWSS_LOG_ERROR("invalid vlan id %u in ethernet frame on %s", vlan_id, info->name.c_str());
            return;
        }

        if (vlan_id == 0)
        {
            // priority packet, frame should be treated as non tagged
            tagged = false;
        }
    }

    // here we have packet to process, we can call some SAI apis
    // TODO call SAI apis
}

static void veth2tap_fun(std::shared_ptr<tap_info_t> info)
{
    SWSS_LOG_ENTER();

    unsigned char buffer[0x4000];

    while (info->run_thread)
    {
        struct msghdr  msg;
        memset(&msg, 0, sizeof(struct msghdr));

        struct sockaddr_storage src_addr;

        struct iovec iov[1];
        iov[0].iov_base = buffer;       // buffer for message
        iov[0].iov_len = sizeof(buffer);

        char control[0x4000];   // buffer for control messages

        msg.msg_name = &src_addr;
        msg.msg_namelen = sizeof(src_addr); // 120
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);

        ssize_t size = recvmsg(info->packet_socket, &msg, 0);

        if (size < 0)
        {
            SWSS_LOG_ERROR("failed to read from socket fd %d, errno(%d): %s",
                    info->packet_socket, errno, strerror(errno));

            usleep(200*1000);

            continue;
        }

        if (msg.msg_controllen < (sizeof(ethhdr)) || msg.msg_controllen > 9000)
        {
            SWSS_LOG_ERROR("INVALID ethernet frame length");
            continue;
        }

        if (msg.msg_namelen <= 0)
        {
            SWSS_LOG_ERROR("ERR didnt get desrciptor\n");
            continue;
        }

        SWSS_LOG_INFO("got message size: %ld, flags: 0x%x namelen: %d ctrllen: %ld, iovlen: %ld",
                size, msg.msg_flags, msg.msg_namelen, msg.msg_controllen,
                iov[0].iov_len);

        struct cmsghdr *cmsg;

        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
                cmsg = CMSG_NXTHDR(&msg, cmsg))
        {
            if (cmsg->cmsg_level == SOL_PACKET && cmsg->cmsg_type == PACKET_AUXDATA)
            {
                printf("CMSG: ");
                for (int i = 0; i <40; i++)
                    printf("%02X ",((unsigned char*)cmsg)[i]);
                printf("\n");

                struct tpacket_auxdata* aux = (struct tpacket_auxdata*)CMSG_DATA(cmsg);

                SWSS_LOG_INFO("GOT CMSG AUX DATA  len: %zu level: %d type: %d , status 0x%x, tpid: 0x%x, tci: 0x%x, vlan: %d",
                        cmsg->cmsg_len, cmsg->cmsg_level, cmsg->cmsg_type,
                        aux->tp_status, aux->tp_vlan_tpid, aux->tp_vlan_tci, aux->tp_vlan_tci & 0xFFF);

                // vlan header
                // TPID(16) | TCI(16) == PCP(3) DEI(1) VID(12)

                if ((aux->tp_status & TP_STATUS_VLAN_VALID) &&
                        (aux->tp_status & TP_STATUS_VLAN_TPID_VALID))
                {
                    memmove(buffer +6+6+4, buffer+6+6, msg.msg_controllen -6 -6); // overlaps, memmove used

                    uint16_t vlan = htons(aux->tp_vlan_tci);
                    uint16_t tpid = htons(0x8100);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
                    uint16_t* pvlan = (uint16_t*)(buffer+6+6);
#pragma GCC diagnostic pop
                    pvlan[0] = tpid;
                    pvlan[1] = vlan;

                    size += 4;
                    break;
                }
            }
        }

        printf("veth2tap: ");
        for (int i = 0; i <fmin(size,20); i++)
            printf("%02X ", buffer[i]);
        printf("\n");

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

static void tap2veth_fun(std::shared_ptr<tap_info_t> info)
{
    SWSS_LOG_ENTER();

    unsigned char buffer[0x4000];

    while (info->run_thread)
    {
        ssize_t size = read(info->tapfd, buffer, sizeof(buffer));

        if (size < 0)
        {
            SWSS_LOG_ERROR("failed to read from tapfd fd %d, errno(%d): %s",
                    info->tapfd, errno, strerror(errno));

            continue;
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
        struct cmsghdr* cmsg = (struct cmsghdr*)buffer;
#pragma GCC diagnostic pop

        printf("tap2veth: ");
        for (size_t i = 0; i <sizeof(struct cmsghdr); i++)
            printf("%02X ", buffer[i]);
        printf("\n");

        if((cmsg->cmsg_level == SOL_PACKET) && (cmsg->cmsg_type == PACKET_AUXDATA))
        {
            // got aux data
            SWSS_LOG_NOTICE("AUX t2e");
        }
        else
        {
            SWSS_LOG_NOTICE(" - no aux data in packet t2e level %d", cmsg->cmsg_level);
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

static int ifup(const char *dev)
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

    if (ifr.ifr_flags & IFF_UP)
    {
        close(s);

        return 0;
    }

    ifr.ifr_flags |= IFF_UP;

    err = ioctl(s, SIOCSIFFLAGS, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCSIFFLAGS on socket %d %s failed, err %d", s, dev, err);
    }

    close(s);

    SWSS_LOG_NOTICE("success ifup on %s", dev);

    return err;
}

static int promisc(const char *dev)
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

    int err = ioctl(s, SIOCGIFFLAGS, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCGIFFLAGS on socket %d %s failed, err %d", s, dev, err);

        close(s);

        return err;
    }

    if (ifr.ifr_flags & IFF_PROMISC)
    {
        close(s);

        return 0;
    }

    ifr.ifr_flags |= IFF_PROMISC;

    err = ioctl(s, SIOCSIFFLAGS, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCSIFFLAGS on socket %d %s failed, err %d", s, dev, err);
    }

    close(s);

    SWSS_LOG_NOTICE("success promisc on %s", dev);

    return err;
}

static bool create_tap_veth_forwarding(
        const std::string &tapname,
        int tapfd)
{
    // we assume here that veth devices were added by user before creating this
    // host interface, veth will be used for packet transfer
    // for example using this:
    // # ip tuntap add name veth7 mode tap
    // # ip a a 10.3.0.1/24 dev veth7
    //

    std::string vethname = "v" + tapname;

    int packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    if (packet_socket < 0)
    {
        SWSS_LOG_ERROR("failed to open packet socket, errno: %d", errno);

        return false;
    }

    int val = 1;

    if (setsockopt(packet_socket, SOL_PACKET, PACKET_AUXDATA, &val, sizeof(val)) < 0)
    {
        SWSS_LOG_ERROR("setsockopt() set PACKET_AUXDATA failed: %s", strerror(errno));
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

    if (ifup(vethname.c_str()))
    {
        SWSS_LOG_ERROR("ifup failed on %s", vethname.c_str());

        close(packet_socket);

        return false;
    }

    if (promisc(vethname.c_str()))
    {
        SWSS_LOG_ERROR("promisc failed on %s", vethname.c_str());

        close(packet_socket);

        return false;
    }

    SWSS_LOG_NOTICE("interface index = %d %s", sock_address.sll_ifindex, vethname.c_str());

    if (bind(packet_socket, (struct sockaddr*) &sock_address, sizeof(sock_address)) < 0)
    {
        SWSS_LOG_ERROR("bind failed on %s", vethname.c_str());

        close(packet_socket);

        return false;
    }

    std::shared_ptr<tap_info_t> info = std::make_shared<tap_info_t>();

    info->packet_socket = packet_socket;
    info->tapfd         = tapfd;
    info->run_thread    = true;
    info->e2t           = std::make_shared<std::thread>(veth2tap_fun, info);
    info->t2e           = std::make_shared<std::thread>(tap2veth_fun, info);
    info->name          = tapname;

    info->e2t->detach();
    info->t2e->detach();

    SWSS_LOG_NOTICE("setup forward rule for %s succeeded", tapname.c_str());

    return true;
}

static int create_tap_device(const char *dev, int flags)
{
    SWSS_LOG_ENTER();

    const char *tundev = "/dev/net/tun";

    int fd = open(tundev, O_RDWR);

    if (fd < 0)
    {
        SWSS_LOG_ERROR("failed to open %s", tundev);
        return -1;
    }

    SWSS_LOG_NOTICE("success opeining %s", tundev);

    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = (short int)flags;  // IFF_TUN or IFF_TAP, IFF_NO_PI

    strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    int err = ioctl(fd, TUNSETIFF, (void *) &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl TUNSETIFF on fd %d %s failed, err %d: %s", fd, dev, err, strerror(err));

        close(fd);

        return err;
    }

    return fd;
}

void setup_dummy_eth_interface()
{
    SWSS_LOG_ENTER();

    const char * dev_name = "eth7";

    SWSS_LOG_NOTICE("creating %s", dev_name);

    int tapfd = create_tap_device(dev_name, IFF_TAP | IFF_MULTI_QUEUE | IFF_NO_PI);

    if (tapfd < 0)
    {
        SWSS_LOG_ERROR("failed to create TAP device for %s", dev_name);
        exit(1);
    }

    if (!create_tap_veth_forwarding(dev_name, tapfd))
    {
        SWSS_LOG_ERROR("forwarding rule on %s was not added", dev_name);
        exit(1);
    }

    if (ifup(dev_name))
    {
        SWSS_LOG_ERROR("ifup failed on %s", dev_name);
        exit(1);
    }

    SWSS_LOG_NOTICE("interface %s setup success", dev_name);
}

static void fun(
        std::shared_ptr<syncd::VendorSai> sai,
        std::shared_ptr<saiproxy::Proxy> proxy)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("staring fun thread");

    // this thred simulates calling SAI api's from DASH app directly
    // while proxy is setup and syncd is sending calls to proxy

    int n = 0;

    while(true)
    {
        sai_attribute_t attr;

        auto sw = proxy->getSwitches();

        if (sw.size() != 0)
        {
            attr.id = SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE;
            attr.value.booldata = true;

            sai_object_id_t switchId = *sw.begin();

            sai_object_id_t vrId = SAI_OBJECT_TYPE_NULL;

            auto status = sai->create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 1, &attr);

            SWSS_LOG_NOTICE("calling SAI api %d, creating virtual router status: %s, vrId: %s",
                    n++,
                    sai_serialize_status(status).c_str(),
                    sai_serialize_object_id(vrId).c_str());

            // NOTE: it's preffered to use VendorSai class for SAI api call's since it's synchronized
            // but since dashapp is linked against libsai which exports sai_* apis then it's possible
            // to call those SAI api's directly for example:

            sai_api_version_t ver = 0;
            status = sai_query_api_version(&ver);

            SWSS_LOG_NOTICE("sai_api_version_query: status: %s: %ld", sai_serialize_status(status).c_str(), ver);
        }
        else
        {
            SWSS_LOG_NOTICE("no switches present in proxy: %d", n++);
        }

        sleep(1);
    }

    SWSS_LOG_ERROR("exiting fun thread");
}

int main(int argc, char** argv)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("dash app started");

    // setup_dummy_eth_interface();

    auto vendorSai = std::make_shared<syncd::VendorSai>();

    auto proxy = std::make_shared<saiproxy::Proxy>(vendorSai);

    std::shared_ptr<std::thread> thread = std::make_shared<std::thread>(fun, vendorSai, proxy);

    proxy->run(); // will block

    return 0;
}
