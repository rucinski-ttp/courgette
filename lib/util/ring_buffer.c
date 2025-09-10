#include "util/ring_buffer.h"

static inline size_t min_size(size_t a, size_t b) { return a < b ? a : b; }

void rb_init(rb_t* rb, uint8_t* storage, size_t size)
{
    rb->buf = storage;
    rb->size = size;
    rb->head = rb->tail = 0;
}

size_t rb_used(const rb_t* rb) { return (rb->head + rb->size - rb->tail) % rb->size; }

size_t rb_free(const rb_t* rb) { return rb->size - rb_used(rb) - 1; /* leave one empty slot */ }

void rb_reset(rb_t* rb) { rb->head = rb->tail = 0; }

size_t rb_write(rb_t* rb, const uint8_t* data, size_t len)
{
    size_t to_write = min_size(len, rb_free(rb));
    for (size_t i = 0; i < to_write; ++i)
    {
        rb->buf[rb->head] = data[i];
        rb->head = (rb->head + 1) % rb->size;
    }
    return to_write;
}

size_t rb_read(rb_t* rb, uint8_t* out, size_t len)
{
    size_t to_read = min_size(len, rb_used(rb));
    for (size_t i = 0; i < to_read; ++i)
    {
        out[i] = rb->buf[rb->tail];
        rb->tail = (rb->tail + 1) % rb->size;
    }
    return to_read;
}

size_t rb_peek_contig(const rb_t* rb, const uint8_t** ptr)
{
    size_t used = rb_used(rb);
    if (used == 0)
    {
        if (ptr)
        {
            *ptr = NULL;
        }
        return 0;
    }
    if (ptr)
    {
        *ptr = &rb->buf[rb->tail];
    }
    if (rb->tail < rb->head)
    {
        return rb->head - rb->tail;
    }
    else
    {
        return rb->size - rb->tail; /* wrap-around region */
    }
}

void rb_consume(rb_t* rb, size_t n)
{
    size_t used = rb_used(rb);
    if (n > used)
    {
        n = used;
    }
    rb->tail = (rb->tail + n) % rb->size;
}
