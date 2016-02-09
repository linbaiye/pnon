#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/if_tun.h>
#include "log.h"
#include "client.h"
#include "protocol.h"

#define SERVER_PORT 6000
#define TUNDEV "tuntest"
#define SERVER_IP "104.238.148.75"
#define MAX_EVENTS 2
#define IFADDR "172.16.1.1"

static struct sockaddr_in server_addr;
static int tunfd = 0;
static int sockfd = 0;


static int tun_alloc_old(char *dev)
{
    char tunname[IFNAMSIZ];

    sprintf(tunname, "/dev/%s", dev);
    return open(tunname, O_RDWR);
}


static int tun_alloc(char *dev)
{
    struct ifreq    ifr;
    int     fd;
    int     err;

    if ((fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK)) < 0)
        return tun_alloc_old(dev);

    memset(&ifr, 0, sizeof(ifr));

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *
     *        IFF_NO_PI - Do not provide packet information
     */
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if (*dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    if ((err = ioctl(fd, TUNSETIFF, (void*)&ifr)) < 0) {
        close(fd);
        printf("error, %s\n", strerror(errno));
        return err;
    }
    return fd;
}


static int tun_config(char *dev)
{
    char buffer[256];
    sprintf(buffer, "ip addr add %s/24 dev %s", IFADDR, dev);
    if (system(buffer) != 0) {
        return -1;
    }
    sprintf(buffer, "ip link set %s up", dev);
    if (system(buffer) != 0) {
        return -1;
    }
    sprintf(buffer, "ip link set mtu 1400 dev %s", dev);
    if (system(buffer) != 0) {
        return -1;
    }
    return 0;
}


static int handle_tun_packet(const char *pkt, int pkt_len)
{
    struct iphdr *iph = (struct iphdr*)pkt;
    struct message msg;
    char buffer[16];
    inet_ntop(AF_INET, &iph->daddr, buffer, 16);
    log_info("Got packet for: %s", buffer);
    if (!prot_encode_message(pkt, pkt_len, &msg)) {
        log_error("Failed to encode message.");
        return -1;
    }
    if (sendto(sockfd, msg.data, msg.data_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to send message.");
        return -1;
    }
    prot_free_message(&msg);
    return 0;
}


static int socket_read(int fd)
{
    struct message msg;
    int ret = prot_decode_message(fd, &msg);
    if (ret < 0) {
        log_error("Failed to decode message.");
        return -1;
    }
    if (ret == 1 || msg.type == MSG_TYPE_PING) {
        return 0;
    }
    if (write(tunfd, msg.payload, msg.payload_len) < 0) {
        log_error("Failed to write tunfd.");
        return -1;
    }
    prot_free_message(&msg);
    return 0;
}


static int init_client_sock(void)
{
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr.s_addr) != 1) {
        return -1;
    }
    server_addr.sin_port = htons(SERVER_PORT);
    return 0;
}


static int tun_read(int fd)
{
    char buffer[1600];
    int len = 0;
again:
    len = read(tunfd, buffer, 1600);
    if (len < 0) {
        if (errno == EINTR) {
            goto again;
        } else {
            log_info("Got error:%s", strerror(errno));
            return -1;
        }
    } else if (len == 0) {
        log_info("Got error:%s", strerror(errno));
        return -1;
    }
    return handle_tun_packet(buffer, len);
}


int event_init(void)
{
    if ((tunfd = tun_alloc(TUNDEV)) < 0) {
        log_error("error, %s\n", strerror(errno));
        return -1;
    }
    if (tun_config(TUNDEV) < 0) {
        log_error("error, %s\n", strerror(errno));
        return -1;
    }
    if (init_client_sock() < 0) {
        log_error("error, %s\n", strerror(errno));
        return -1;
    }
    if ((sockfd = socket(AF_INET, SOCK_NONBLOCK | SOCK_DGRAM, 0)) < 0) {
        log_error("Failed to create socket.");
        return -1;
    }
    return 0;
}

static int send_ping(void)
{
    struct message msg;
    if (!prot_encode_ping(&msg)) {
        log_error("Failed to encode ping.");
        return -1;
    }
    if (sendto(sockfd, msg.data, msg.data_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to write socket fd:%s.", strerror(errno));
        return -1;
    }
    log_info("Sent ping.");
    return 0;
}

int event_loop(void)
{
    fd_set rd_set;
    int len = 0;
    struct timeval tv;
    int maxfd = tunfd > sockfd ? tunfd + 1 : sockfd + 1;
    while (1) {
        FD_ZERO(&rd_set);
        FD_SET(tunfd, &rd_set);
        FD_SET(sockfd, &rd_set);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        int ret = select(maxfd, &rd_set, NULL, NULL, &tv);
        if (ret < 0 && errno == EINTR) {
            continue;
        } else if (ret < 0) {
            log_error("select returned error.");
            return -1;
        } else if (ret == 0) {
            if (send_ping() < 0) {
                return -1;
            }
        }
        if (FD_ISSET(tunfd, &rd_set)) {
            if (tun_read(tunfd) < 0) {
                log_error("Got error:%s", strerror(errno));
                return -1;
            }
        } else if (FD_ISSET(sockfd, &rd_set)) {
            if (socket_read(sockfd) < 0) {
                log_error("socket read error.");
                return -1;
            }
        }
    }
    return 0;
}
