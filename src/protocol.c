#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "log.h"

#define MAX_PAYLOAD_LEN 1460
#define MAX_BUFFER_LEN 3200

struct {
    char buffer[MAX_BUFFER_LEN];
    int counter;
} read_buffer;

static uint32_t msgid;


void prot_free_message(struct message *msg)
{
    if (msg && msg->data) {
        free(msg->data);
        msg->data = NULL;
    }
}


/*
 * |-> length <-|-> type <-|-> msg id <-|-> payload <-|
 * The length does not include the size of the length field.
 */
struct message *prot_encode_message(const char *payload, int payload_len, struct message *msg)
{
    if (!payload || payload_len < 0 || payload_len > MAX_PAYLOAD_LEN || !msg) {
        return NULL;
    }
    msg->data_len = payload_len + sizeof(uint32_t) + sizeof(uint32_t) + 1;
    msg->data = malloc(msg->data_len);
    if (!msg->data) {
        return NULL;
    }
    msg->msgid = msgid++;
    uint32_t net_uint32 = htonl(payload_len + sizeof(uint32_t) + 1);
    int ptr = 0;
    memcpy(msg->data + ptr, &net_uint32, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    msg->data[ptr++] = MSG_TYPE_DATA;
    net_uint32 = htonl(msg->msgid);
    memcpy(msg->data + ptr, &net_uint32, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(msg->data + ptr, payload, payload_len);
    return msg;
}


struct message *prot_encode_ping(struct message *msg)
{
    if (!msg) {
        return NULL;
    }
    msg->data_len = sizeof(uint32_t) + 1 + sizeof(uint32_t);
    msg->data = malloc(msg->data_len);
    if (!msg->data) {
        return NULL;
    }
    uint32_t net_uint32 = htonl(sizeof(uint32_t) + 1);
    int ptr = 0;
    memcpy(msg->data + ptr, &net_uint32, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    msg->data[ptr++] = MSG_TYPE_PING;
    msg->msgid = msgid++;
    net_uint32 = htonl(msg->msgid);
    memcpy(msg->data + ptr, &net_uint32, sizeof(uint32_t));
    return msg;
}


static int decode_message(uint32_t msg_len, struct message *msg)
{
    msg->payload_len = msg_len - sizeof(uint32_t) - 1;
    if (msg->payload_len > 0) {
        msg->payload = malloc(msg->payload_len);
        if (!msg->payload) {
            return -1;
        }
    }
    int ptr = sizeof(uint32_t);
    msg->type = *((uint8_t *)(read_buffer.buffer + ptr));
    ++ptr;
    msg->msgid = ntohl(*((uint32_t *)(read_buffer.buffer + ptr)));
    if (msg->payload_len > 0) {
        ptr += sizeof(uint32_t);
        memcpy(msg->payload, read_buffer.buffer + ptr, msg->payload_len);
    }
    return 0;
}


static void drain_buffer(uint32_t msg_len)
{
    for (int i = 0, j = msg_len + sizeof(uint32_t); j < read_buffer.counter; i++, j++) {
        read_buffer.buffer[i] = read_buffer.buffer[j];
    }
    read_buffer.counter -= msg_len + sizeof(uint32_t);
}

static void dump_peer_info(struct sockaddr_in *addr)
{
    char buffer[16];
    inet_ntop(AF_INET, &addr->sin_addr.s_addr, buffer, 16);
    log_info("Got a packet from :[%s:%d].", buffer, ntohs(addr->sin_port));
}


int prot_decode_message(int fd, struct message *msg)
{
    int socklen = sizeof(struct sockaddr_in);
    if (msg == NULL) {
        return -1;
    }
    int len = 0;
again:
    memset(&msg->peer, 0, sizeof(struct sockaddr_in));
    len = recvfrom(fd, read_buffer.buffer, MAX_BUFFER_LEN - read_buffer.counter, 0, (struct sockaddr*)&msg->peer, &socklen);
    if (len < 0) {
        if (errno == EINTR) {
            goto again;
        } else {
            return -1;
        }
    }
    dump_peer_info(&msg->peer);
    read_buffer.counter += len;
    if (read_buffer.counter >= sizeof(uint32_t)) {
        uint32_t msg_len = ntohl(*((uint32_t *)read_buffer.buffer));
        if (msg_len <= read_buffer.counter - sizeof(uint32_t)) {
            if (decode_message(msg_len, msg) < 0) {
                return -1;
            }
            drain_buffer(msg_len);
            return 0;
        }
    }
    return 1;
}


void prot_init(void)
{
    read_buffer.counter = 0;
    msgid = 0;
}
