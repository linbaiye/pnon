#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ringbuffer.h"


struct ringbuffer *rb_new(int32_t cap)
{
    if (cap <= 0 || cap > RB_MAX_LEN) {
        return NULL;
    }
    struct ringbuffer * tmp = malloc(sizeof(struct ringbuffer));
    if (!tmp) {
        return NULL;
    }
    /* Add one byte to indicate if the buffer is full or empty. */
    tmp->capacity = cap + 1;
    tmp->buffer = malloc(tmp->capacity);
    if (!tmp->buffer) {
        free(tmp);
        return NULL;
    }
    tmp->head = tmp->buffer;
    tmp->rear = tmp->buffer;
    return tmp;
}


static int32_t calc_unused_space(struct ringbuffer *rb, int continuous)
{
    if (rb->rear < rb->head) {
        return rb->head - rb->rear - 1;
    }
    /* We want a continuous range of space. */
    if (continuous == 1) {
        if (rb->head == rb->buffer) {
            return (rb->buffer + rb->capacity) - rb->rear - 1;
        }
        return rb->buffer + rb->capacity - rb->rear;
    }
    return ((rb->buffer + rb->capacity) - rb->rear) + (rb->head - rb->buffer) - 1;
}


int32_t rb_available(struct ringbuffer *rb)
{
    if (!rb) {
        return -1;
    }
    return calc_unused_space(rb, 0);
}


/*
 * Return value:
 * @ >0, the actual bytes being read.
 * @-1, an error occurrs.
 * @0, the buffer is full.
 * Param:
 * @peer, if this is not null, the routine will use recvfrom() to get the peer's
 * socket info and store it in @peer.
 */
int rb_recvfrom(int fd, struct ringbuffer *rb, struct sockaddr_in *peer)
{
    if (!rb) {
        return -1;
    }
    int copied = 0;
    while (1) {
        int32_t size = calc_unused_space(rb, 1);
        if (size == 0) {
            break;
        }
        int len = 0;
        if (peer) {
            int socklen = sizeof(struct sockaddr_in);
            len = recvfrom(fd, rb->rear, size, 0, (struct sockaddr *)peer, &socklen);
        } else {
            len = read(fd, rb->rear, size);
        }
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            return (errno == EAGAIN || errno == EWOULDBLOCK) ? copied : -1;
        } else if (len == 0) {
            return -1;
        }
        rb->rear += len;
        if (rb->rear == rb->buffer + rb->capacity) {
            rb->rear = rb->buffer;
        }
        copied += len;
    }
    return copied;
}


int rb_read(int fd, struct ringbuffer *rb)
{
    if (!rb) {
        return -1;
    }
    return rb_recvfrom(fd, rb, NULL);
}


static int32_t calc_used_space(struct ringbuffer *rb, int continuous)
{
    if (rb->rear >= rb->head) {
        return rb->rear - rb->head;
    }
    if (continuous) {
        return (rb->buffer + rb->capacity) - rb->head;
    }
    return (rb->buffer + rb->capacity) - rb->head + (rb->rear - rb->buffer);
}


int32_t rb_get_used_bytes(struct ringbuffer *rb)
{
    if (!rb) {
        return -1;
    }
    return calc_used_space(rb, 0);
}


static int32_t drain_cached_bytes(struct ringbuffer *rb, void *dst,
        int32_t size, int fd, struct sockaddr_in *peer)
{
    int32_t copied = 0;
    while (1) {
        int32_t bytes_to_copy = calc_used_space(rb, 1);
        if (bytes_to_copy == 0 || size == 0) {
            break;
        }
        bytes_to_copy = bytes_to_copy > size ? size: bytes_to_copy;
        if (dst != NULL) {
            memcpy((char *)dst + copied, rb->head, bytes_to_copy);
        } else {
            int ret;
            if (peer != NULL) {
                ret = sendto(fd, rb->head, bytes_to_copy, 0,
                        (struct sockaddr*)peer, sizeof(struct sockaddr_in));
            } else {
                ret = write(fd, rb->head, bytes_to_copy);
            }
            if (ret <= 0) {
                if (errno == EINTR) {
                    continue;
                }
                return -1;
            }
            bytes_to_copy = ret;
        }
        rb->head += bytes_to_copy;
        if (rb->head == rb->buffer + rb->capacity) {
            rb->head = rb->buffer;
        }
        size -= bytes_to_copy;
        copied += bytes_to_copy;
    }
    return copied;
}


int32_t rb_drain(struct ringbuffer *rb, void *dst, int32_t size)
{
    if (!rb || !dst || size <= 0) {
        return -1;
    }
    return drain_cached_bytes(rb, dst, size, -1, NULL);
}


int32_t rb_copy(struct ringbuffer *rb, void *dst, int32_t size)
{
    if (!rb || !dst || size <= 0) {
        return -1;
    }
    char *saved_head = rb->head;
    int32_t copied = rb_drain(rb, dst, size);
    rb->head = saved_head;
    return copied;
}


int32_t rb_sendto(int fd, struct ringbuffer *rb, int32_t size, struct sockaddr_in *peer)
{
    if (!rb || size <= 0 || !peer) {
        return -1;
    }
    return drain_cached_bytes(rb, NULL, size, fd, peer);
}

int32_t rb_write(int fd, struct ringbuffer *rb, int32_t size)
{
    if (!rb || size <= 0) {
        return -1;
    }
    return drain_cached_bytes(rb, NULL, size, fd, NULL);
}
