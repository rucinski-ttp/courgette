#include "util/crc32.h"

/* CRC-32 (IEEE 802.3) polynomial 0xEDB88320 */

static uint32_t crc32_table[256];
static int crc32_init_done;

static void crc32_init_table(void)
{
    if (crc32_init_done)
        return;
    for (uint32_t i = 0; i < 256; ++i)
    {
        uint32_t c = i;
        for (uint32_t j = 0; j < 8; ++j)
        {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_init_done = 1;
}

uint32_t crc32_update(uint32_t crc, const void* data, size_t len)
{
    crc32_init_table();
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < len; ++i)
    {
        crc = crc32_table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc;
}
