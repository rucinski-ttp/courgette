#include <string.h>

#include "app/display.h"
#include "app/serial_async.h"
#include "cmd/dispatch.h"
#include "protocol/protocol.h"
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>

static uint16_t le16(const uint8_t* p) { return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8u); }
static void wr_le16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8u) & 0xFFu);
}

static int h_info(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req;
    (void)req_len;
    const display_info_t* di = display_get_info();
    if (!di || !rsp || !rsp_len || *rsp_len < 8)
    {
        return -1;
    }
    wr_le16(&rsp[0], di->width);
    wr_le16(&rsp[2], di->height);
    rsp[4] = (uint8_t)di->format;
    rsp[5] = di->can_read ? 1u : 0u;
    rsp[6] = di->can_write ? 1u : 0u;
    if (*rsp_len >= 10)
    {
        wr_le16(&rsp[8], di->stride_bytes);
        *rsp_len = 10;
    }
    else
    {
        *rsp_len = 8; /* legacy: no stride in 8-byte response */
    }
    return 0;
}

static int h_fill(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp;
    (void)rsp_len;
    if (req_len < 3)
    {
        return -1;
    }
    return display_fill_rgb(req[0], req[1], req[2]);
}

static int h_read(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    if (req_len < 8 || !rsp || !rsp_len)
    {
        return -1;
    }
    uint16_t x = le16(&req[0]);
    uint16_t y = le16(&req[2]);
    uint16_t w = le16(&req[4]);
    uint16_t h = le16(&req[6]);
    /* Limit maximum returned bytes to avoid overrunning protocol buffers */
    const display_info_t* di = display_get_info();
    if (!di)
    {
        return -1;
    }
    uint32_t bytes = (uint32_t)w * (uint32_t)h;
    uint8_t bpp = 2;
    switch (di->format)
    {
    case DISP_FMT_RGB565:
        bpp = 2;
        break;
    case DISP_FMT_RGB888:
        bpp = 3;
        break;
    case DISP_FMT_ARGB8888:
        bpp = 4;
        break;
    case DISP_FMT_BGR565:
        bpp = 2;
        break;
    case DISP_FMT_BGR888:
        bpp = 3;
        break;
    }
    bytes *= bpp;
    if (bytes > *rsp_len)
    {
        return -1;
    }
    int rc = display_read_rect(x, y, w, h, rsp, *rsp_len);
    if (rc < 0)
    {
        return rc;
    }
    *rsp_len = bytes;
    return 0;
}

static int h_get_id(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req;
    (void)req_len;
    if (!rsp || !rsp_len || *rsp_len == 0)
    {
        return -1;
    }
    int n = display_get_panel_id(rsp, *rsp_len);
    if (n < 0)
    {
        *rsp_len = 0;
        return 0; /* Treat as optional: return empty payload on failure */
    }
    *rsp_len = (uint32_t)n;
    return 0;
}

static int h_blank_off(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    ARG_UNUSED(req);
    ARG_UNUSED(req_len);
    if (rsp_len)
    {
        *rsp_len = 0;
    }
    const display_info_t* di = display_get_info();
    if (!di)
    {
        return -ENODEV;
    }
    /* Nudge the driver to unblank/backlight on */
    /* We can't access internal device; call fill(black) to cause write path, then unblank */
    (void)display_fill_rgb(0, 0, 0);
    /* Try blanking off via API directly */
    const struct device* dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!dev || !device_is_ready(dev))
    {
        return -ENODEV;
    }
    return display_blanking_off(dev);
}

static int h_blank_on(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    ARG_UNUSED(req);
    ARG_UNUSED(req_len);
    if (rsp_len)
    {
        *rsp_len = 0;
    }
    const struct device* dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!dev || !device_is_ready(dev))
    {
        return -ENODEV;
    }
    return display_blanking_on(dev);
}

static int h_diag(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    ARG_UNUSED(req);
    ARG_UNUSED(req_len);
    if (!rsp || !rsp_len || *rsp_len < 16)
    {
        return -EMSGSIZE;
    }
    /* Build a small text report */
    int n = 0;
    const struct device* ltdc = DEVICE_DT_GET(DT_NODELABEL(ltdc));
    const struct device* dsi = DEVICE_DT_GET(DT_NODELABEL(mipi_dsi));
#if DT_NODE_EXISTS(DT_NODELABEL(otm8009a))
    const struct device* pan = DEVICE_DT_GET(DT_NODELABEL(otm8009a));
#else
    const struct device* pan = NULL;
#endif
    n += snprintk((char*)&rsp[n], (int)(*rsp_len - (uint32_t)n), "ltdc:%s dsi:%s pan:%s\n",
                  ltdc && device_is_ready(ltdc) ? "ok" : "na",
                  dsi && device_is_ready(dsi) ? "ok" : "na",
                  pan && device_is_ready(pan) ? "ok" : "na");
    if (n < (int)*rsp_len)
    {
        const display_info_t* di = display_get_info();
        if (di)
        {
            n += snprintk((char*)&rsp[n], (int)(*rsp_len - (uint32_t)n),
                          "res:%ux%u fmt:%u stride:%u\n", (unsigned)di->width, (unsigned)di->height,
                          (unsigned)di->format, (unsigned)di->stride_bytes);
        }
    }
    if (n < (int)*rsp_len)
    {
        rsp[n++] = 0; /* NUL */
    }
    *rsp_len = (uint32_t)n;
    return 0;
}

int cmd_display_init(void)
{
    int rc = display_init();
    /* Even if display init fails, register handlers so tests can detect status */
    (void)cmd_register(CMD_ID_DISP_INFO, h_info);
    (void)cmd_register(CMD_ID_DISP_FILL, h_fill);
    (void)cmd_register(CMD_ID_DISP_READ, h_read);
    (void)cmd_register(CMD_ID_DISP_GET_ID, h_get_id);
    (void)cmd_register(CMD_ID_DISP_BLANK_OFF, h_blank_off);
    (void)cmd_register(CMD_ID_DISP_BLANK_ON, h_blank_on);
    (void)cmd_register(CMD_ID_DISP_DIAG, h_diag);
    const char* msg = NULL;
    char buf[64];
    if (rc == 0)
    {
        const display_info_t* di = display_get_info();
        /* Default boot state: show white to confirm panel is alive. */
        (void)display_fill_rgb(255, 255, 255);
        /* Also explicitly unblank and set brightness on panel device if present */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(otm8009a), okay)
        const struct device* pan = DEVICE_DT_GET(DT_NODELABEL(otm8009a));
        if (pan && device_is_ready(pan))
        {
            (void)display_blanking_off(pan);
            /* Best-effort brightness to max */
            (void)display_set_brightness(pan, 0xFF);
        }
#endif
        if (di)
        {
            /* [DISPLAY] ok WxH fmt=F */
            int n = 0;
            n += snprintk(&buf[n], (int)(sizeof(buf) - n), "[DISPLAY] ok %ux%u fmt=%u",
                          (unsigned)di->width, (unsigned)di->height, (unsigned)di->format);
            buf[sizeof(buf) - 1] = 0;
            msg = buf;
        }
    }
    else
    {
        int n = 0;
        n += snprintk(&buf[n], (int)(sizeof(buf) - n), "[DISPLAY] init failed rc=%d", rc);
        buf[sizeof(buf) - 1] = 0;
        msg = buf;
    }
    if (msg)
    {
        uint8_t frame[96];
        size_t flen = 0;
        proto_msg_t m = {.cmd = CMD_ID_LOG,
                         .flags = 0x02,
                         .payload = (const uint8_t*)msg,
                         .length = (uint32_t)strlen(msg)};
        if (proto_encode(&m, frame, sizeof(frame), &flen) == PROTO_OK)
        {
            (void)platform_serial_write(frame, flen);
        }
    }
    return rc;
}

/* Reserved: place holder for optional panel-specific init if needed in future. */
