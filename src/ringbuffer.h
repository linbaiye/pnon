#ifndef _RINGBUFFER_H_
#define _RINGBUFFER_H_
#define RB_MAX_LEN (16 << 10)
struct ringbuffer {
    char *buffer;
    char *head;
    char *rear;
    int32_t capacity;
};
extern struct ringbuffer *rb_new(int32_t size);
extern struct ringbuffer *rb_read(int fd, struct ringbuffer *rb);
extern int32_t rb_length(struct ringbuffer *rb);
extern int32_t rb_copy(struct ringbuffer *rb, void *dst, int32_t size);
extern int32_t rb_drop(struct ringbuffer *rb, int32_t size);
#endif
