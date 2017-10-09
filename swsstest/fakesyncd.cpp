#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
//#include <linux/if.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>

#include "swss/logger.h"

#include "sai.h"

#include <thread>

#include <pcap.h>
#include <stdint.h>

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
        SWSS_LOG_ERROR("ioctl SIOCSIFHWADDR on socked %d %s failed, err %d", s, dev, err);
    }

    close(s);

    return err;
}

int ifup(const char *dev)
{
    int sockfd;
    struct ifreq ifr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0)
        return -1;

    memset(&ifr, 0, sizeof ifr);

    strncpy(ifr.ifr_name, dev , IFNAMSIZ);

    ifr.ifr_flags |= IFF_UP;

    return ioctl(sockfd, SIOCSIFFLAGS, &ifr);
}

int promisc(int socket, const char *dev)
{
    struct ifreq ifr;

    strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);

    int ret = ioctl(socket, SIOCGIFFLAGS, &ifr);

    if (ret < 0);
    {
        SWSS_LOG_ERROR("failed to get SIOCGIFFLAGS for %s", dev);

        return ret;
    }

    ifr.ifr_flags |= IFF_PROMISC;

    ret = ioctl(socket, SIOCSIFFLAGS, &ifr);

    if (ret < 0)
    {
        SWSS_LOG_ERROR("failed to set SIOCSIFFLAGS for %s", dev);

        return ret;
    }

    return 0;
}

void thread_fun(int tapidx, int tapfd, int packet_socket)
{
    std::string vethname = "sw1eth" + std::to_string(tapidx);
    std::string tapname = "Ethernet" + std::to_string(tapidx); // could be port channel

    const char *dev = vethname.c_str();
    const char *tapdev = tapname.c_str();

    // TODO read from socket

    printf("started packet forward for %s\n", dev);

    unsigned char buffer[0x4000];

    while (1)
    {
        ssize_t ret = read(packet_socket, buffer, sizeof(buffer));

        if (ret < 0)
        {
            printf("failed to read from socket");
            break;
        }

        if (buffer[0] != 0x33 &&buffer[1]!= 0x33)
        {
            printf("send %s -> %s ", dev, tapdev);

            printf("rd ");
            for (int j = 0; j < (int)ret; ++j)
                printf("%02x", buffer[j]);
            printf("\n");
        }

        int wr = write(tapfd, buffer, ret);

        if (wr < 0)
        {
            printf("failed to write to interface\n");
            break;
        }
    }
}

void thread_fun_tap(int devidx, int tapfd, int packet_socket)
{
    std::string vethname = "sw1eth" + std::to_string(devidx);
    std::string tapname = "Ethernet" + std::to_string(devidx); // could be port channel

    const char *dev = vethname.c_str();

    unsigned char buffer[0x4000];

    while (1)
    {
        int nread = read(tapfd, buffer, sizeof(buffer));

        if (nread < 0)
        {
            perror("Reading from interface");

            return;
        }

        if (buffer[0] != 0x33 &&buffer[1]!= 0x33)
        {
            printf("send %s -> %s ", tapname.c_str(), vethname.c_str());

            printf("wr ");
            for (int j = 0; j < nread; ++j)
              printf("%02x", buffer[j]); // also arp reply
            printf("\n");
        }


        int wr = write(packet_socket, buffer, nread);

        if (wr < 0)
        {
            printf("failed to write packet to %s\n", dev);
            break;
        }
    }
}

int main()
{
    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);

    for (int i = 0; i < 128; i+=4)
    {
        std::string name = "Ethernet" + std::to_string(i);

        // create TAP device

        SWSS_LOG_INFO("creating hostif %s", name.c_str());

        int tapfd = vs_create_tap_device(name.c_str(), IFF_TAP| IFF_MULTI_QUEUE | IFF_NO_PI);

        if (tapfd < 0)
        {
            SWSS_LOG_ERROR("failed to create TAP device for %s", name.c_str());

            return SAI_STATUS_FAILURE;
        }

        SWSS_LOG_INFO("created TAP device for %s, fd: %d", name.c_str(), tapfd);

        sai_mac_t mac = { 0, 0x11, 0x11, 0x11, 0x11, 0x22 };

        int err = vs_set_dev_mac_address(name.c_str(), mac);

        if (err < 0)
        {
            SWSS_LOG_ERROR("failed to set MAC address for %s",
                    name.c_str());

            close(tapfd);

            return SAI_STATUS_FAILURE;
        }

        ifup(name.c_str());

        int packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

        if (packet_socket < 0)
        {
            printf("failed to open packet socket, errno: %d\n", errno);

            exit(1);
        }

        std::string vethname = "sw1eth" + std::to_string(i);

        //if (promisc(packet_socket, name.c_str()) < 0)
        //{
        //    printf("failed to set promisc mode on %s\n", name.c_str());
        //    exit(1);
        //}

        // bind to device

        struct sockaddr_ll sock_address;

        memset(&sock_address, 0, sizeof(sock_address));

        sock_address.sll_family = PF_PACKET;
        sock_address.sll_protocol = htons(ETH_P_ALL);
        sock_address.sll_ifindex = if_nametoindex(vethname.c_str());

        if (sock_address.sll_ifindex == 0)
        {
            printf("failed to get interface index for %s\n", vethname.c_str());
            continue;
        }

        printf("index = %d %s\n", sock_address.sll_ifindex, vethname.c_str());

        if (bind(packet_socket, (struct sockaddr*) &sock_address, sizeof(sock_address)) < 0)
        {
            printf("bind failed on %s\n", vethname.c_str());

            exit(1);
        }

        printf("packet socked opened for %s\n", name.c_str());

        std::thread th(thread_fun, i, tapfd, packet_socket);

        th.detach();

        std::thread tapth(thread_fun_tap, i, tapfd, packet_socket);

        tapth.detach();
    }

    // not just sleep and read/write from tap interfaces

    printf("setup interfaces success\n");

    sleep(1000);
}
