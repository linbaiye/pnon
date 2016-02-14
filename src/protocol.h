#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_
#include <netinet/in.h>
enum {
    MSG_TYPE_ACK = 0,
    MSG_TYPE_DATA,
    MSG_TYPE_PING,
} MSG_TYPES;
struct sockaddr_in;
struct message {
    uint32_t msgid;
    char *data;              /* Point to the whole message, including header. */
    uint32_t data_len;
    uint8_t type;
    struct sockaddr_in peer;
    char *payload;
    uint32_t payload_len;
};
extern struct message *prot_encode_message(const char *payload, int payload_len, struct message *);
extern struct message *prot_encode_ping(struct message *);
extern void prot_init(void);
extern int prot_decode_message(int fd, struct message *msg);
extern void prot_free_message(struct message *msg);
#define MAX_PAYLOAD_LEN 1460
#endif
