#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Pack 8-bit per channel RGB to 16-bit RGB565 (big endian independent). */
    static inline uint16_t pixel_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
    {
        return (uint16_t)(((uint16_t)(r & 0xF8u) << 8u) | ((uint16_t)(g & 0xFCu) << 3u) |
                          ((uint16_t)(b) >> 3u));
    }

    static inline uint16_t pixel_rgb888_to_bgr565(uint8_t r, uint8_t g, uint8_t b)
    {
        /* BGR565: blue in bits 15..11, green 10..5, red 4..0 */
        return (uint16_t)(((uint16_t)(b & 0xF8u) << 8u) | ((uint16_t)(g & 0xFCu) << 3u) |
                          ((uint16_t)(r) >> 3u));
    }

#ifdef __cplusplus
}
#endif
