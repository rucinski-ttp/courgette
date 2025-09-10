#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define BUFPOOL_CHUNK_SIZE 1024u
#define BUFPOOL_COUNT 6u

    void bufpool_init(void);
    uint8_t* bufpool_acquire(uint32_t* out_cap);
    void bufpool_release(uint8_t* p);

#ifdef __cplusplus
}
#endif
