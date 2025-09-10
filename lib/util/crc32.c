#include "util/crc32.h"

/* CRC-32 update. On-device delegate to Zephyr's IEEE implementation; on host,
 * use a portable bitwise fallback. Seed with 0xFFFFFFFF; finalize with ^ 0xFFFFFFFF.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __ZEPHYR__
uint32_t crc32_ieee_update(uint32_t crc, const uint8_t* data, size_t len);
uint32_t crc32_update(uint32_t crc, const void* data, size_t len)
{
    return crc32_ieee_update(crc, (const uint8_t*)data, len);
}
#else
uint32_t crc32_update(uint32_t crc, const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= p[i];
        for (int b = 0; b < 8; ++b)
        {
            uint32_t lsb = crc & 1u;
        crc >>= 1u;
            if (lsb)
            {
                crc ^= 0xEDB88320u;
            }
        }
    }
    return crc;
}
#endif
