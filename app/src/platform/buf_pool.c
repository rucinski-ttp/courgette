#include "app/buf_pool.h"
#include <string.h>
#include <zephyr/kernel.h>

typedef struct
{
    uint8_t mem[BUFPOOL_CHUNK_SIZE];
} bufchunk_t;

static bufchunk_t s_pool[BUFPOOL_COUNT];
static struct k_mutex s_mu;
static uint32_t s_bitmap;

void bufpool_init(void)
{
    k_mutex_init(&s_mu);
    s_bitmap = 0;
}

uint8_t* bufpool_acquire(uint32_t* out_cap)
{
    k_mutex_lock(&s_mu, K_FOREVER);
    for (uint32_t i = 0; i < BUFPOOL_COUNT; ++i)
    {
        if ((s_bitmap & (1u << i)) == 0)
        {
            s_bitmap |= (1u << i);
            k_mutex_unlock(&s_mu);
            if (out_cap)
            {
                *out_cap = BUFPOOL_CHUNK_SIZE;
            }
            return s_pool[i].mem;
        }
    }
    k_mutex_unlock(&s_mu);
    if (out_cap)
    {
        *out_cap = 0;
    }
    return NULL;
}

void bufpool_release(uint8_t* p)
{
    if (!p)
    {
        return;
    }
    k_mutex_lock(&s_mu, K_FOREVER);
    for (uint32_t i = 0; i < BUFPOOL_COUNT; ++i)
    {
        if (p == s_pool[i].mem)
        {
            s_bitmap &= ~(1u << i);
            break;
        }
    }
    k_mutex_unlock(&s_mu);
}
