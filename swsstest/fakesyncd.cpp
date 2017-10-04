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

#include "swss/logger.h"

#include "sai.h"

#include <thread>

#define MAX_INTERFACE_NAME_LEN IFNAMSIZ

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

void thread_fun(int tapidx, int tapfd)
{
    // TODO for each interface we need swXethX for EthernetY
    // that will transport packets in both directions

    std::string vethname = "sw1eth" + std::to_string(tapidx);
    std::string tapname = "Ethernet" + std::to_string(tapidx); // could be port channel

    const char *dev = vethname.c_str();
    const char *tapdev = tapname.c_str();

    char errbuf[PCAP_ERRBUF_SIZE];

    pcap_t *handle;

    // or use raw sockets https://gist.github.com/austinmarton/2862515
    handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);

    if (handle == NULL)
    {
        SWSS_LOG_ERROR("Couldn't open device %s: %s\n", dev, errbuf);

        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        return;
    }

    printf("started packet forward for %s\n", dev);

    pcap_t *fp;

    fp = pcap_open_live(tapdev, BUFSIZ, 1, 1000, errbuf);

    if (fp == NULL)
    {
        SWSS_LOG_ERROR("Couldn't open device %s: %s\n", tapdev, errbuf);

        fprintf(stderr, "Couldn't open device %s: %s\n", tapdev, errbuf);
        return;
    }

    // TODO start receiving thread
    // TODO later on we could have only 2 threads for all interfaces

    while (1)
    {
        struct pcap_pkthdr header;

        unsigned const char * packet = pcap_next(handle, &header); // blocking for 1 sec timeout
    
        if (packet == NULL)
        {
            continue;
        }

        // TODO check packet source mac address and potentially
        // send fdb_event notification

        printf("got packet len %d on %s\n", header.len, dev);

        for (int j = 0; j < header.len; ++j)
        {
            printf("%02x", packet[j]);
        }

        printf("\n");

        int wr = write(tapfd, packet, header.len);

        printf("wr %d\n", wr);
        //continue;

        //// inject packet to tap device
        //int ret;
        //if ((ret = pcap_sendpacket(fp, packet, header.len) )== -1)
        //{
        //    printf("FAILED to send packet on %s\n", tapdev);
        //}

        //printf("sent packet %d to %s\n", ret, tapdev);
    }

    printf("exit thread %s\n", dev);


    pcap_close(handle);
}

void thread_fun_tap(int devidx, int tapfd)
{
    char buffer[0x2000];

    while (1)
    {
        int nread = read(tapfd, buffer, sizeof(buffer));

        if (nread < 0)
        {
            perror("Reading from interface");
            
            return;
        }

        printf("Read %d bytes from device %d\n", nread, devidx);
    }
}

int main()
{
    SWSS_LOG_ENTER();

    for (int i = 0; i < 128; i+=4)
    {
        std::string name = "Ethernet" + std::to_string(i);

        // create TAP device

        SWSS_LOG_INFO("creating hostif %s", name.c_str());

        int tapfd = vs_create_tap_device(name.c_str(), IFF_TAP| IFF_MULTI_QUEUE|IFF_NO_PI);

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

         std::thread th(thread_fun, i, tapfd);

         th.detach();

         std::thread tapth(thread_fun_tap, i, tapfd);

         tapth.detach();
    }

    // not just sleep and read/write from tap interfaces

    printf("setup interfaces success\n");

    sleep(1000);
}
