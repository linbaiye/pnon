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

void conn_fd_set(struct connection *c, fd_set *set)
{
    if (!set || !c) {
        log_error("Bad param.");
        return;
    }
    FD_SET(c->udp_fd, set);
    FD_SET(c->tcp_fd, set);
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


static int conn_write(const char *buffer, uint32_t len)
{
    if (!buffer || len == 0 || len > MAX_PAYLOAD_LEN) {
        log_info("Invalid param.");
        return -1;
    }
    return 0;
}


int conn_read(struct connection *c, fd_set *set, char **buffer, uint32_t *len)
{
    if (FD_ISSET(c->tcp_fd, set)) {
        ;
    }
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
    conn->state = CONN_CLOSED;
    return conn;
}
