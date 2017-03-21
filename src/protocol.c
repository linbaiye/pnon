#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "log.h"


static uint32_t msgid = 0;

void prot_free_message(struct message *msg)
{
    if (msg && msg->data) {
        free(msg->data);
        msg->data = NULL;
    }
}


static int encode_header(struct message *msg, int payload_len, uint8_t type)
{
    msg->data_len = payload_len + PROT_HDR_LEN;
    msg->data = malloc(msg->data_len);
    if (!msg->data) {
        return -1;
    }
    msg->msgid = msgid++;
    uint32_t net_uint32 = htonl(msg->data_len);
    int ptr = 0;
    memcpy(msg->data + ptr, &net_uint32, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    msg->data[ptr++] = PROT_CUR_VER;
    msg->data[ptr++] = type;
    net_uint32 = htonl(msg->msgid);
    memcpy(msg->data + ptr, &net_uint32, sizeof(uint32_t));
    return 0;
}


/*
 * |-> length <-|-> version <-|-> type <-|-> msg id <-|-> payload <-|
 */
struct message *prot_encode_message(const char *payload, int payload_len, struct message *msg)
{
    if (!payload || payload_len <= 0 || payload_len > PROT_MTU || !msg) {
        return NULL;
    }
    if (encode_header(msg, payload_len, PROT_TYPE_DATA) < 0) {
        return NULL;
    }
    memcpy(msg->data + PROT_HDR_LEN, payload, payload_len);
    return msg;
}


struct message *prot_encode_ping(struct message *msg)
{
    if (!msg) {
        return NULL;
    }
    if (encode_header(msg, 0, PROT_TYPE_PING) < 0) {
        return NULL;
    }
    return msg;
}



int prot_has_complete_message(const char *buff, uint32_t buff_len)
{
    if (!buff || buff_len < PROT_HDR_LEN) {
        return 0;
    }
    uint32_t msg_len = ntohl(*((uint32_t *)buff));
    return buff_len >= msg_len;
}


/*
 * Return value:
 * @-1 bad param passed or malloc() fails.
 * @0, it's a malformed message.
 * @>0, we've got a full message and the length of the message is returned.
 */
int prot_decode_message(struct message *msg, const char *buff, uint32_t buff_len)
{
    if (!msg || !prot_has_complete_message(buff, buff_len)) {
        return -1;
    }
    uint32_t msg_len = ntohl(*((uint32_t *)buff));
    msg->data = malloc(msg_len);
    if (!msg->data) {
        return -1;
    }
    msg->msg_len = msg_len;
    memcpy(msg->data, buff, msg->msg_len);
    int ptr = sizeof(uint32_t);
    msg->version = (uint8_t)msg->data[ptr++];
    if (msg->version != PROT_CUR_VER) {
        free(msg->data);
        return 0;
    }
    msg->type = (uint8_t)msg->data[ptr++];
    if (msg->type != PROT_TYPE_DATA && msg->type != PROT_TYPE_PING) {
        free(msg->data);
        return 0;
    }
    msg->msgid = ntohl(*((uint32_t *)msg->data));
    ptr += sizeof(uint32_t);
    if (msg->type == PROT_TYPE_DATA) {
        msg->payload = msg->data + ptr;
        msg->payload_len = msg->data_len - PROT_HDR_LEN;
        if (msg->payload_len == 0) {
            free(msg->data);
            return 0;
        }
    }
    return msg->data_len;
}


void prot_init(void)
{
    msgid = 0;
}
