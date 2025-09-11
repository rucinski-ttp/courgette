#include "app/display.h"

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/mipi_dsi.h>
#include <zephyr/kernel.h>

#include "display/pixel.h"

static const struct device* s_disp_dev;
static display_info_t s_info;

static inline uint8_t bpp_for_format(display_pixel_format_t f)
{
    switch (f)
    {
    case DISP_FMT_RGB565:
        return 2;
    case DISP_FMT_RGB888:
        return 3;
    case DISP_FMT_ARGB8888:
        return 4;
    default:
        return 2;
    }
}

int display_init(void)
{
    s_disp_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!s_disp_dev || !device_is_ready(s_disp_dev))
    {
        return -ENODEV;
    }

    struct display_capabilities cap;
    memset(&cap, 0, sizeof(cap));
    display_get_capabilities(s_disp_dev, &cap);

    s_info.width = (uint16_t)cap.x_resolution;
    s_info.height = (uint16_t)cap.y_resolution;
    /* Determine read capability by checking driver API, as screen_info lacks a read flag */
    const struct display_driver_api* api = (const struct display_driver_api*)s_disp_dev->api;
    s_info.can_read = (api && api->read != NULL);
    s_info.can_write = true;

    /* Honor the driver's current pixel format to avoid mismatches with shield config */
    switch (cap.current_pixel_format)
    {
    case PIXEL_FORMAT_BGR_565:
        s_info.format = DISP_FMT_BGR565;
        break;
    case PIXEL_FORMAT_RGB_565:
        s_info.format = DISP_FMT_RGB565;
        break;
    case PIXEL_FORMAT_ARGB_8888:
        s_info.format = DISP_FMT_ARGB8888;
        break;
    case PIXEL_FORMAT_RGB_888:
        s_info.format = DISP_FMT_RGB888;
        break;
    default:
        /* Try to pick a sane default if current not reported */
        if (cap.supported_pixel_formats & PIXEL_FORMAT_RGB_888)
        {
            (void)display_set_pixel_format(s_disp_dev, PIXEL_FORMAT_RGB_888);
            s_info.format = DISP_FMT_RGB888;
        }
        else if (cap.supported_pixel_formats & PIXEL_FORMAT_RGB_565)
        {
            (void)display_set_pixel_format(s_disp_dev, PIXEL_FORMAT_RGB_565);
            s_info.format = DISP_FMT_RGB565;
        }
        else if (cap.supported_pixel_formats & PIXEL_FORMAT_BGR_565)
        {
            (void)display_set_pixel_format(s_disp_dev, PIXEL_FORMAT_BGR_565);
            s_info.format = DISP_FMT_BGR565;
        }
        else if (cap.supported_pixel_formats & PIXEL_FORMAT_ARGB_8888)
        {
            (void)display_set_pixel_format(s_disp_dev, PIXEL_FORMAT_ARGB_8888);
            s_info.format = DISP_FMT_ARGB8888;
        }
        else
        {
            /* Fallback */
            s_info.format = DISP_FMT_RGB565;
        }
        break;
    }
    s_info.stride_bytes = (uint16_t)(s_info.width * (uint16_t)bpp_for_format(s_info.format));

    /* Ensure panel is unblanked */
    (void)display_blanking_off(s_disp_dev);

    return 0;
}

const display_info_t* display_get_info(void) { return &s_info; }

int display_fill_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_disp_dev)
    {
        return -ENODEV;
    }
    const uint8_t bpp = bpp_for_format(s_info.format);
    /* Build a single scanline of the selected color */
    uint16_t w = s_info.width;
    uint8_t line[1024];
    if ((uint32_t)w * bpp > sizeof(line))
    {
        /* For wide resolutions, send in chunks */
        w = (uint16_t)(sizeof(line) / bpp);
    }
    struct display_buffer_descriptor desc = {
        .width = w,
        .height = 1,
        .pitch = w,
        .buf_size = w * bpp,
    };

    /* Encode one line of color */
    if (s_info.format == DISP_FMT_RGB565)
    {
        uint16_t px = pixel_rgb888_to_rgb565(r, g, b);
        for (uint16_t i = 0; i < w; ++i)
        {
            line[2u * i + 0u] = (uint8_t)(px >> 8u);
            line[2u * i + 1u] = (uint8_t)(px & 0xFFu);
        }
    }
    else if (s_info.format == DISP_FMT_BGR565)
    {
        uint16_t px = pixel_rgb888_to_bgr565(r, g, b);
        for (uint16_t i = 0; i < w; ++i)
        {
            line[2u * i + 0u] = (uint8_t)(px >> 8u);
            line[2u * i + 1u] = (uint8_t)(px & 0xFFu);
        }
    }
    else if (s_info.format == DISP_FMT_ARGB8888)
    {
        /* LTDC expects 32-bit ARGB pixel words; on LE memory the byte order
         * for direct writes is typically B,G,R,A. Using that ordering here
         * matches the STM32 LTDC driver behavior. */
        for (uint16_t i = 0; i < w; ++i)
        {
            line[4u * i + 0u] = b;    /* B */
            line[4u * i + 1u] = g;    /* G */
            line[4u * i + 2u] = r;    /* R */
            line[4u * i + 3u] = 0xFF; /* A */
        }
    }
    else /* RGB888 */
    {
        /* BRG ordering observed on A09 path: write B,R,G */
        for (uint16_t i = 0; i < w; ++i)
        {
            line[3u * i + 0u] = b; /* B */
            line[3u * i + 1u] = r; /* R */
            line[3u * i + 2u] = g; /* G */
        }
    }

    /* Write each scanline across the display */
    for (uint16_t y = 0; y < s_info.height; ++y)
    {
        for (uint16_t x = 0; x < s_info.width; x += w)
        {
            uint16_t ww = (uint16_t)((x + w <= s_info.width) ? w : (s_info.width - x));
            desc.width = ww;
            desc.pitch = ww;
            desc.buf_size = ww * bpp;
            int rc = display_write(s_disp_dev, x, y, &desc, line);
            if (rc != 0)
            {
                return rc;
            }
        }
    }
    return 0;
}

int display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* src,
                        uint16_t src_stride_px)
{
    if (!s_disp_dev)
    {
        return -ENODEV;
    }
    const uint8_t bpp = bpp_for_format(s_info.format);
    if (s_info.format != DISP_FMT_RGB565)
    {
        /* Convert line by line */
        uint8_t line[1024];
        if ((uint32_t)w * bpp > sizeof(line))
        {
            return -EINVAL;
        }
        struct display_buffer_descriptor desc = {
            .width = w, .height = 1, .pitch = w, .buf_size = w * bpp};
        for (uint16_t yy = 0; yy < h; ++yy)
        {
            if (s_info.format == DISP_FMT_ARGB8888)
            {
                for (uint16_t i = 0; i < w; ++i)
                {
                    uint16_t px = src[i + (uint32_t)yy * src_stride_px];
                    uint8_t rr = (uint8_t)((px >> 8u) & 0xF8u);
                    uint8_t gg = (uint8_t)((px >> 3u) & 0xFCu);
                    uint8_t bb = (uint8_t)((px << 3u) & 0xF8u);
                    line[4u * i + 0u] = bb;   /* B */
                    line[4u * i + 1u] = gg;   /* G */
                    line[4u * i + 2u] = rr;   /* R */
                    line[4u * i + 3u] = 0xFF; /* A */
                }
            }
            else /* RGB888 */
            {
                for (uint16_t i = 0; i < w; ++i)
                {
                    uint16_t px = src[i + (uint32_t)yy * src_stride_px];
                    line[3u * i + 0u] = (uint8_t)((px << 3u) & 0xF8u); /* B */
                    line[3u * i + 1u] = (uint8_t)((px >> 8u) & 0xF8u); /* R */
                    line[3u * i + 2u] = (uint8_t)((px >> 3u) & 0xFCu); /* G */
                }
            }
            int rc = display_write(s_disp_dev, x, (uint16_t)(y + yy), &desc, line);
            if (rc != 0)
            {
                return rc;
            }
        }
        return 0;
    }
    /* Native RGB565: can write directly line-by-line */
    struct display_buffer_descriptor desc = {
        .width = w, .height = 1, .pitch = w, .buf_size = w * bpp};
    for (uint16_t yy = 0; yy < h; ++yy)
    {
        const uint8_t* line = (const uint8_t*)&src[(uint32_t)yy * src_stride_px];
        int rc = display_write(s_disp_dev, x, (uint16_t)(y + yy), &desc, line);
        if (rc != 0)
        {
            return rc;
        }
    }
    return 0;
}

int display_read_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t* out,
                      uint32_t out_len)
{
    if (!s_disp_dev)
    {
        return -ENODEV;
    }
    const uint32_t need = (uint32_t)w * (uint32_t)h * bpp_for_format(s_info.format);
    if (out_len < need)
    {
        return -EMSGSIZE;
    }
    /* Use driver's read implementation if available */
    const uint8_t bpp = bpp_for_format(s_info.format);
    struct display_buffer_descriptor desc = {
        .width = w, .height = h, .pitch = w, .buf_size = w * bpp * h};
    int rc = display_read(s_disp_dev, x, y, &desc, out);
    if (rc != 0)
    {
        return rc;
    }
    if (s_info.format == DISP_FMT_ARGB8888)
    {
        uint32_t pixels = (uint32_t)w * (uint32_t)h;
        for (uint32_t i = 0; i < pixels; ++i)
        {
            uint8_t b = out[4u * i + 0u];
            uint8_t g = out[4u * i + 1u];
            uint8_t r = out[4u * i + 2u];
            uint8_t a = out[4u * i + 3u];
            out[4u * i + 0u] = a; /* A */
            out[4u * i + 1u] = r; /* R */
            out[4u * i + 2u] = g; /* G */
            out[4u * i + 3u] = b; /* B */
        }
    }
    return 0;
}

int display_get_panel_id(uint8_t* out, uint32_t max_len)
{
#if DT_NODE_HAS_STATUS(DT_NODELABEL(mipi_dsi), okay)
    const struct device* dsi = DEVICE_DT_GET(DT_NODELABEL(mipi_dsi));
    if (!dsi || !device_is_ready(dsi))
    {
        return -ENODEV;
    }
    if (!out || max_len == 0)
    {
        return -EINVAL;
    }
    /* DCS Get Display ID (0x04) typically returns up to 3 bytes */
    size_t len = max_len;
    if (len > 8)
    {
        len = 8; /* cap */
    }
    ssize_t rc = mipi_dsi_dcs_read(dsi, 0 /* channel */, 0x04u, out, len);
    if (rc < 0)
    {
        return (int)rc;
    }
    return (int)rc;
#else
    (void)out;
    (void)max_len;
    return -ENOTSUP;
#endif
}
