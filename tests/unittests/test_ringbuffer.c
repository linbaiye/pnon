#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <arpa/inet.h>
#include "ringbuffer.c"


int __wrap_recvfrom(int fd, void *buf, size_t buf_len, int flags, struct sockaddr *addr, int *socklen)
{
    return mock_type(int);
}


static void test_rb_recvfrom(void **state)
{
    struct ringbuffer *rb = rb_new(10);
    struct sockaddr_in peer;
    will_return(__wrap_recvfrom, 10);
    assert_true(rb_recvfrom(1, rb, &peer) == 10);
    rb->head = rb->rear = rb->buffer;
    will_return(__wrap_recvfrom, 4);
    will_return(__wrap_recvfrom, -1);
    assert_true(rb_recvfrom(1, rb, &peer) == -1);
    rb->head = rb->buffer + 2;
    rb->rear = rb->buffer + 1;
    assert_true(rb_recvfrom(1, rb, &peer) == 0);
    rb->rear = rb->buffer + rb->capacity - 1;
    rb->head = rb->buffer + rb->capacity - 2;
    will_return(__wrap_recvfrom, 1);
    will_return(__wrap_recvfrom, 2);
    will_return(__wrap_recvfrom, -1);
    errno = EAGAIN;
    assert_true(rb_recvfrom(1, rb, &peer) == 3);
    rb->rear = rb->buffer + rb->capacity - 1;
    rb->head = rb->buffer + 2;
    will_return(__wrap_recvfrom, 1);
    will_return(__wrap_recvfrom, -1);
    assert_true(rb_recvfrom(1, rb, &peer) == 1);
    assert_true(rb->rear == rb->buffer);
}


static void test_calc_unused_space(void **state)
{
    struct ringbuffer *rb = rb_new(3);
    assert_true(calc_unused_space(rb, 1) == 3);
    assert_true(calc_unused_space(rb, 0) == 3);
    rb->rear++;
    assert_true(calc_unused_space(rb, 1) == 2);
    assert_true(calc_unused_space(rb, 0) == 2);

    rb->rear = rb->buffer + rb->capacity - 1;
    assert_true(calc_unused_space(rb, 1) == 0);
    assert_true(calc_unused_space(rb, 0) == 0);

    rb->head = rb->buffer + 1;
    rb->rear = rb->buffer;
    assert_true(calc_unused_space(rb, 1) == 0);
    assert_true(calc_unused_space(rb, 0) == 0);

    rb->head = rb->buffer + rb->capacity;
    rb->rear = rb->buffer;
    assert_true(calc_unused_space(rb, 1) == 3);
    assert_true(calc_unused_space(rb, 0) == 3);

    rb->head = rb->buffer;
    rb->rear = rb->buffer + 1;
    assert_true(calc_unused_space(rb, 0) == 2);
    assert_true(calc_unused_space(rb, 1) == 2);

    rb->head = rb->buffer + 1;
    rb->rear = rb->buffer + 2;
    assert_true(calc_unused_space(rb, 0) == 2);
    assert_true(calc_unused_space(rb, 1) == 2);

    rb->head = rb->buffer;
    rb->rear = rb->buffer + 2;
    assert_true(calc_unused_space(rb, 0) == 1);
    assert_true(calc_unused_space(rb, 1) == 1);
}


static void test_calc_used_space(void **state)
{
    struct ringbuffer *rb = rb_new(5);
    assert_true(calc_used_space(rb, 0) == 0);
    assert_true(calc_used_space(rb, 1) == 0);
    rb->rear = rb->buffer + rb->capacity - 1;
    assert_true(calc_used_space(rb, 0) == 5);
    assert_true(calc_used_space(rb, 1) == 5);
    rb->rear = rb->buffer;
    rb->head = rb->buffer + rb->capacity - 1;
    assert_true(calc_used_space(rb, 0) == 1);
    assert_true(calc_used_space(rb, 1) == 1);
    rb->rear = rb->buffer + 2;
    rb->head = rb->buffer + 3;
    assert_true(calc_used_space(rb, 0) == 5);
    assert_true(calc_used_space(rb, 1) == 3);
}


static void test_rb_drain(void **state)
{
    struct ringbuffer *rb = rb_new(5);
    for (int i = 0; i< 5; i++, rb->rear++) {
        *rb->rear = 'a' + i;
    }
    char buffer[10];
    int32_t ret = rb_drain(rb, buffer, 10);
    assert_true(ret == 5 && strncmp(buffer, "abcde", 5) == 0);
    rb->head = rb->buffer + 3;
    rb->rear = rb->buffer + 3;
    for (int i = 3; i <= 5; i++, rb->rear++) {
        *rb->rear = 'a' + i;
    }
    rb->rear = rb->buffer;
    for (int i = 0; i < 2; i++, rb->rear++) {
        *rb->rear = 'a' + i;
    }
    ret = rb_drain(rb, buffer, 5);
    assert_true(ret == 5 && strncmp(buffer, "defab", 5) == 0);
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_calc_used_space),
        cmocka_unit_test(test_calc_unused_space),
        cmocka_unit_test(test_rb_recvfrom),
        cmocka_unit_test(test_rb_drain),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
