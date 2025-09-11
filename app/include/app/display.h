#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        DISP_FMT_RGB565 = 0,
        DISP_FMT_RGB888 = 1,
        DISP_FMT_ARGB8888 = 2,
        DISP_FMT_BGR565 = 3,
        DISP_FMT_BGR888 = 4,
    } display_pixel_format_t;

    typedef struct
    {
        uint16_t width;
        uint16_t height;
        uint16_t stride_bytes; /* bytes per line in native format */
        display_pixel_format_t format;
        bool can_read;
        bool can_write;
    } display_info_t;

    /* Initialize the display subsystem and select a pixel format. */
    int display_init(void);

    /* Query selected mode */
    const display_info_t* display_get_info(void);

    /* Fill entire screen with an RGB color (8-bit per channel). */
    int display_fill_rgb(uint8_t r, uint8_t g, uint8_t b);

    /* Blit a rectangle of RGB565 pixels at (x,y) with given width/height and source stride in
     * pixels. */
    int display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* src,
                            uint16_t src_stride_px);

    /* Read back a rectangle into out buffer in native format (size must be w*h*bytes_per_pixel). */
    int display_read_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t* out,
                          uint32_t out_len);

    /* Optional: Read panel ID via MIPI DCS 0x04 (Get Display ID). Returns bytes read or <0 on
     * error. */
    int display_get_panel_id(uint8_t* out, uint32_t max_len);

#ifdef __cplusplus
}
#endif
