#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_
#include <
enum {
    PROT_TYPE_DATA = 0,
    PROT_TYPE_PING,
} PROT_TYPES;

struct message {
    uint32_t data_len;       /* The length of the whole message. */
    uint8_t version;
    uint8_t type;
    uint32_t msgid;
    char *data;              /* Point to the whole message, including header. */
    char *payload;           /* Where the payload begins. */
    uint32_t payload_len;
};
extern struct message *prot_encode_message(const char *payload, int payload_len, struct message *);
extern struct message *prot_encode_ping(struct message *);
extern int prot_decode_message(int fd, struct message *msg);
extern void prot_free_message(struct message *msg);
extern int prot_has_complete_message(const char *buffer, int buff_len);
#define PROT_MTU 1400
#define PROT_CUR_VER 0
#define PROT_HDR_LEN (sizeof(uint32_t) * 2 + sizeof(uint8_t) * 2)
#define prot_message_len(m) (m->data_len)
#define PROT_MAX_MSG_LEN PROT_HDR_LEN + PROT_MTU
#endif
