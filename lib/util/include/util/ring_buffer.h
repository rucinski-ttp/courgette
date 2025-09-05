#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        uint8_t* buf;
        size_t size;
        size_t head;
        size_t tail;
    } rb_t;

    void rb_init(rb_t* rb, uint8_t* storage, size_t size);
    size_t rb_used(const rb_t* rb);
    size_t rb_free(const rb_t* rb);
    void rb_reset(rb_t* rb);
    size_t rb_write(rb_t* rb, const uint8_t* data, size_t len);
    size_t rb_read(rb_t* rb, uint8_t* out, size_t len);

#ifdef __cplusplus
}
#endif
