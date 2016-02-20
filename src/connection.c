#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdint.h>
#include <connection.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include "log.h"
#include "protocol.h"
#include "ringbuffer.h"

#define BUFFER_SIZE (6 << 10)

void conn_fd_set(struct connection *c, fd_set *set, fd_set *wset)
{
    if (!c) {
        log_error("Bad param.");
        return;
    }
    if (set) {
        FD_SET(c->udp_fd, set);
        FD_SET(c->tcp_fd, set);
    }
    if (wset) {
        FD_SET(c->udp_fd, wset);
        FD_SET(c->tcp_fd, wset);
    }
}


static int init_sockaddr(struct sockaddr_in *peer, const char *ip, uint16_t port)
{
    memset(peer, 0, sizeof(struct sockaddr_in));
    peer->sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &peer->sin_addr.s_addr) != 1) {
        return -1;
    }
    peer->sin_port = htons(port);
}


int conn_connect(struct connection *c)
{
    int ret = connect(c->tcp_fd, (struct sockaddr *)&c->tcp_peer, sizeof(struct sockaddr_in));
    if (ret == 0) {
        c->state == CONN_ESTABLISHED;
    }
    return ret;
}


int conn_finish_connection(struct connection *c, fd_set *rset, fd_set *wset)
{
    if (!c || !rset || !wset) {
        return -1;
    }
    if (FD_ISSET(c->tcp_fd, rset) || FD_ISSET(c->tcp_fd, wset)) {
        int error, len = sizeof(int);
        if (getsockopt(c->tcp_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            return -1;
        }
        if (error) {
            errno = error;
            return -1;
        }
        return 0;
    }
    return -1;
}


static int conn_write(const char *buffer, uint32_t len)
{
    if (!buffer || len == 0 || len > MAX_PAYLOAD_LEN) {
        log_info("Invalid param.");
        return -1;
    }
    return 0;
}


int conn_close(struct connection *c)
{
    if (c->tcp_err != 0) {
        close(c->tcp_fd);
        rb_reset(c->tcp_rbuffer);
        c->tcp_err = 0;
        c->state = CONN_CLOSED;
    }
    if (c->udp_err != 0) {
        rb_reset(c->udp_rbuffer);
        c->udp_err = 0;
    }
}


static int try_decoding_message(struct connection *c, struct message *msg, int is_tcp)
{
    uint8_t buffer[PROT_MAX_MSG_LEN];
    struct ringbuffer *rb = c->udp_rbuffer;
    int ret, *err = &c->udp_err;
    if (is_tcp) {
        ret = rb_read(c->tcp_fd, c->tcp_rbuffer);
        rb = c->tcp_rbuffer;
        err = &c->tcp_err;
    } else {
        ret = rb_recvfrom(c->udp_fd, c->udp_rbuffer, &c->udp_peer);
    }
    *err = 0;
    if (ret < 0) {
        *err = EPIPE;
        return -1;
    } else if (ret == 0 && !rb_is_full(rb)) {
        return 0;
    }
    int len = rb_copy(rb, buffer, PROT_MAX_MSG_LEN);
    if ((ret = prot_decode_message(msg, buffer, len)) < 0) {
        *err = ENOMEM;
        return -1;
    } else if (ret == 0) {
        return 0;
    }
    rb_drop(rb, len);
    return 1;
}


int conn_read(struct connection *c, fd_set *set, struct message **msgs)
{
    if (!c || !set || !msgs) {
        return -1;
    }
    int count = 0;
    if (FD_ISSET(c->tcp_fd, set)) {
        if (try_decoding_message(c, msgs[count], 1) > 0) {
            count++;
        }
    }
    if (FD_ISSET(c->udp_fd, set)) {
        if (try_decoding_message(c, msgs[count], 0) > 0) {
            count++;
        }
    }
    return count;
}


static struct connection *alloc_connection(void)
{
    struct connection *conn = malloc(sizeof(struct connection));
    if (!conn) {
        log_error("Failed to malloc connection.");
        return NULL;
    }
    conn->tcp_rbuffer = malloc(BUFFER_SIZE);
    if (!conn->tcp_rbuffer) {
        free(conn);
        log_error("Failed to new tcp buffer.");
        return NULL;
    }
    conn->udp_rbuffer = malloc(BUFFER_SIZE);
    if (!conn->udp_rbuffer) {
        free(conn->tcp_rbuffer);
        free(conn);
        log_error("Failed to new udp buffer.");
        return NULL;
    }
    memset(&conn->udp_peer, 0, sizeof(struct sockaddr_in));
    memset(&conn->tcp_peer, 0, sizeof(struct sockaddr_in));
    conn->udp_fd = -1;
    conn->tcp_fd = -1;
    conn->state == CONN_CLOSED;
    conn->vip = 0;
    conn->tcp_err = 0;
    conn->upd_err = 0;
    return conn;
}


void conn_free(struct connection *c)
{
    if (!c) {
        return;
    }
    free(c->udp_rbuffer);
    free(c->tcp_rbuffer);
    free(c);
}


struct connection *conn_new(const char *ip, uint32_t udp_port, uint32_t tcp_port)
{
    if (!ip) {
        log_error("ip is null.");
        return NULL;
    }
    struct connection *conn = alloc_connection();
    if (!conn) {
        return NULL;
    }
    if (init_sockaddr(&conn->udp_peer, ip, udp_port) < 0) {
        log_error("Failed init udp sockaddr.");
        conn_free(conn);
        return NULL;
    }
    if (init_sockaddr(&conn->tcp_peer, ip, tcp_port) < 0) {
        log_error("Failed init tcp sockaddr.");
        conn_free(conn);
        return NULL;
    }
    if ((conn->tcp_fd = socket(AF_INET, SOCK_NONBLOCK | SOCK_STREAM, 0)) < 0) {
        log_error("Failed to alloc socket.");
        conn_free(conn);
        return NULL;
    }
    if ((conn->udp_fd = socket(AF_INET, SOCK_NONBLOCK | SOCK_DGRAM, 0)) < 0) {
        log_error("Failed to alloc socket.");
        conn_free(conn);
        return NULL;
    }
    return conn;
}
