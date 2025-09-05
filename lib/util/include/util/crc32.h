#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Streaming CRC-32 (IEEE 802.3) helpers.
     * Usage:
     *   uint32_t crc = 0xFFFFFFFFu;
     *   crc = crc32_update(crc, data_part1, len1);
     *   crc = crc32_update(crc, data_part2, len2);
     *   crc = crc32_finalize(crc);
     */
    uint32_t crc32_update(uint32_t crc, const void* data, size_t len);
    static inline uint32_t crc32_finalize(uint32_t crc) { return crc ^ 0xFFFFFFFFu; }

#ifdef __cplusplus
}
#endif
