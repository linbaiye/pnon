#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <arpa/inet.h>
#include "protocol.c"

static void test_prot_encode_message(void **state)
{
    prot_init();
    char *payload = "test";
    struct message *msg = prot_encode_message(payload, strlen(payload));
    assert_true(msg != NULL && msg->msgid == 0);
    uint32_t value = ntohl(*((uint32_t *)msg->data));
    assert_true(value == strlen(payload) + sizeof(uint32_t) + 1);
    uint8_t type = ((uint8_t *)msg->data)[sizeof(uint32_t)];
    assert_true(type == MSG_TYPE_DATA);
    value = ntohl(*((uint32_t *)(msg->data + sizeof(uint32_t) + 1)));
    assert_true(value == msg->msgid);
    assert_true(strncmp(payload, msg->data + sizeof(uint32_t) * 2 + 1, strlen(payload)) == 0);
}


static void test_decode_message(void **state)
{
    prot_init();
    char *payload = "test";
    struct message *msg = prot_encode_message(payload, strlen(payload));
    memcpy(read_buffer.buffer, msg->data, msg->data_len);
    read_buffer.counter = msg->data_len;
    struct message tmp;
    assert_true(decode_message(ntohl(*(uint32_t *)read_buffer.buffer), &tmp) == 0);
    assert_true(tmp.payload_len == strlen(payload));
    assert_true(strcmp(tmp.payload, payload) == 0);
    assert_true(tmp.type == MSG_TYPE_DATA);
}


int __wrap_recvfrom(int fd, void *buf, size_t buf_len, int flags, struct sockaddr *addr, int *socklen)
{
    return mock_type(int);
}


static void test_drain_buffer(void **state)
{
    prot_init();
    char *payload = "test";
    struct message *msg = prot_encode_message(payload, strlen(payload));
    memcpy(read_buffer.buffer, msg->data, msg->data_len);
    uint32_t pad = htonl(1024);
    read_buffer.counter += msg->data_len;
    memcpy(read_buffer.buffer + read_buffer.counter, &pad, sizeof(uint32_t));
    read_buffer.counter += sizeof(uint32_t);
    drain_buffer(msg->data_len - sizeof(uint32_t));
    assert_true(read_buffer.counter == sizeof(uint32_t));
    assert_true(ntohl(*(uint32_t *)read_buffer.buffer) == 1024);
}


static void test_prot_decode_message(void **state)
{
    prot_init();
    char *payload = "test";
    struct message *msg = prot_encode_message(payload, strlen(payload));
    memcpy(read_buffer.buffer, msg->data, msg->data_len);
    will_return(__wrap_recvfrom, msg->data_len);
    struct message tmp;
    assert_true(prot_decode_message(1, &tmp) == 0);
    assert_true(read_buffer.counter == 0);
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_prot_encode_message),
        cmocka_unit_test(test_decode_message),
        cmocka_unit_test(test_drain_buffer),
        cmocka_unit_test(test_prot_decode_message),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
