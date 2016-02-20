#ifndef _CONNECTION_H_
#define _CONNECTION_H_

enum CONN_STATE {
    CONN_CLOSED = 0,
    CONN_ESTABLISHED,
    CONN_UNKNOWN
};
#include <arpa/inet.h>

struct sockaddr_in;
struct ringbuffer;

struct connection {
    struct sockaddr_in udp_peer;
    struct sockaddr_in tcp_peer;
    struct ringbuffer *udp_rbuffer;
    struct ringbuffer *tcp_rbuffer;
    int vip;                       /* Virtual ip. */
    int udp_fd;
    int tcp_fd;
    int state;
    int tcp_err;
    int udp_err;
};


#endif
