#include <string.h>
#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#endif

#include "app/activity_led.h"
#include "app/sd_ops.h"
#include "app/serial_async.h"
#include "cmd/dispatch.h"
#include "protocol/protocol.h"

#ifndef __ZEPHYR__
#include <stdio.h>
#ifndef snprintk
#define snprintk snprintf
#endif
/* Host builds do not need special nocache attributes; avoid reserved identifiers */
#else
#include <zephyr/linker/section_tags.h>
#endif

/* Staging buffer for responses populated by SD ops worker. Place in regular
 * BSS to avoid any potential overlap with special nocache regions. */
static uint8_t sd_dma_buf[1024];

/* Payload formats (little-endian):
 * - FORMAT: no payload
 * - LIST: path\0 -> response: lines "D name\n" or "F name size\n"
 * - READ: path\0 [u32 offset][u32 len] -> rsp: data bytes (<= len)
 * - WRITE: path\0 [u32 offset][u32 len] [data]
 * - RENAME: old\0 new\0
 * - DELETE: path\0
 * - MKDIR: path\0
 * - STAT: path\0 -> rsp: [u32 size][u32 flags] (bit0=dir)
 * - CHECKSUM: path\0 -> rsp: [u32 crc32]
 * - PEEK: path\0 [u32 offset][u32 len] -> rsp: data bytes (<= len)
 */

static const char* parse_cstr(const uint8_t* p, uint32_t n, uint32_t* idx)
{
    uint32_t i = *idx;
    const char* s = (const char*)&p[i];
    while (i < n && p[i] != '\0')
    {
        i++;
    }
    if (i >= n)
    {
        return NULL;
    }
    *idx = i + 1;
    return s;
}

static int copy_cstr_local(char* dst, size_t cap, const uint8_t* p, uint32_t n, uint32_t* idx)
{
    uint32_t i = *idx;
    if (!dst || cap == 0)
    {
        return -1;
    }
    size_t w = 0;
    while (i < n && p[i] != '\0')
    {
        if (w + 1 < cap)
        {
            dst[w++] = (char)p[i];
        }
        i++;
    }
    if (i >= n)
    {
        return -1; /* no NUL terminator */
    }
    dst[w] = '\0';
    *idx = i + 1;
    return 0;
}
static uint32_t le32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) | ((uint32_t)p[2] << 16u) |
           ((uint32_t)p[3] << 24u);
}
static void wr_le32(uint8_t* p, uint32_t v)
{
    p[0] = v & 0xFFu;
    p[1] = (v >> 8u) & 0xFFu;
    p[2] = (v >> 16u) & 0xFFu;
    p[3] = (v >> 24u) & 0xFFu;
}

static int h_format(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req;
    (void)req_len;
    (void)rsp;
    (void)rsp_len;
    activity_led_pulse();
    /* Asynchronous format to avoid blocking the protocol thread. */
    return sd_format();
}

static int h_list(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    if (!rsp || !rsp_len)
    {
        return -1;
    }
    uint32_t idx = 0;
    char pbuf[128];
    if (copy_cstr_local(pbuf, sizeof(pbuf), req, req_len, &idx) != 0)
    {
        return -1;
    }
    uint32_t cap = *rsp_len;
    /* Use DMA-safe staging buffer then copy into rsp */
    uint32_t sc = cap;
    if (sc > sizeof(sd_dma_buf))
    {
        sc = sizeof(sd_dma_buf);
    }
    int rc = sd_list(pbuf, sd_dma_buf, &sc);
    if (rc == 0)
    {
        memcpy(rsp, sd_dma_buf, sc);
        *rsp_len = sc;
    }
    return rc;
}

static int h_read(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    uint32_t idx = 0;
    char pbuf[128];
    if (copy_cstr_local(pbuf, sizeof(pbuf), req, req_len, &idx) != 0 || (idx + 8) > req_len ||
        !rsp || !rsp_len)
    {
        return -1;
    }
    uint32_t offset = le32(&req[idx]);
    idx += 4;
    uint32_t len = le32(&req[idx]);
    idx += 4;
    /* Cap read to available response buffer; aim to satisfy entire request in one frame. */
    uint32_t cap = *rsp_len;
    if (len < cap)
    {
        cap = len;
    }
    int rc = sd_read(pbuf, offset, rsp, &cap);
    if (rc == 0)
    {
        *rsp_len = cap;
    }
    return rc;
}

static int h_write(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp;
    if (rsp_len)
    {
        *rsp_len = 0;
    }
    uint32_t idx = 0;
    char pbuf[128];
    if (copy_cstr_local(pbuf, sizeof(pbuf), req, req_len, &idx) != 0)
    {
        return -1;
    }
    if ((idx + 8) > req_len)
    {
        return -1;
    }
    uint32_t offset = le32(&req[idx]);
    idx += 4;
    uint32_t len = le32(&req[idx]);
    idx += 4;
    if (idx + len > req_len)
    {
        return -1;
    }
    /* quiet */
    activity_led_pulse();
    if (len > sizeof(sd_dma_buf))
    {
        return -1;
    }
    /* Copy payload to a stable staging buffer before handing to worker to avoid
     * lifetime issues with the parser's internal buffer. */
    memcpy(sd_dma_buf, &req[idx], len);
    return sd_write(pbuf, offset, sd_dma_buf, len);
}

static int h_rename(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp;
    if (rsp_len)
    {
        *rsp_len = 0;
    }
    uint32_t idx = 0;
    char ab[128], bb[128];
    if (copy_cstr_local(ab, sizeof(ab), req, req_len, &idx) != 0)
    {
        return -1;
    }
    if (copy_cstr_local(bb, sizeof(bb), req, req_len, &idx) != 0)
    {
        return -1;
    }
    return sd_rename(ab, bb);
}

static int h_delete(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp;
    if (rsp_len)
    {
        *rsp_len = 0;
    }
    uint32_t idx = 0;
    char ab[128];
    if (copy_cstr_local(ab, sizeof(ab), req, req_len, &idx) != 0)
    {
        return -1;
    }
    return sd_delete(ab);
}

static int h_mkdir(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp;
    if (rsp_len)
    {
        *rsp_len = 0;
    }
    uint32_t idx = 0;
    char ab[128];
    if (copy_cstr_local(ab, sizeof(ab), req, req_len, &idx) != 0)
    {
        return -1;
    }
    /* quiet */
    return sd_mkdir(ab);
}

static int h_stat(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    if (!rsp || !rsp_len || *rsp_len < 8)
    {
        return -1;
    }
    uint32_t idx = 0;
    char ab[128];
    if (copy_cstr_local(ab, sizeof(ab), req, req_len, &idx) != 0)
    {
        return -1;
    }
    /* quiet */
    uint32_t size = 0;
    int is_dir = 0;
    int rc = sd_stat_size(ab, &size, &is_dir);
    if (rc == 0)
    {
        wr_le32(&rsp[0], size);
        wr_le32(&rsp[4], (uint32_t)(is_dir ? 1 : 0));
        *rsp_len = 8;
    }
    return rc;
}

static int h_checksum(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    if (!rsp || !rsp_len || *rsp_len < 4)
    {
        return -1;
    }
    uint32_t idx = 0;
    char ab[128];
    if (copy_cstr_local(ab, sizeof(ab), req, req_len, &idx) != 0)
    {
        return -1;
    }
    uint32_t crc = 0;
    int rc = sd_checksum_crc32(ab, &crc);
    if (rc == 0)
    {
        wr_le32(rsp, crc);
        *rsp_len = 4;
    }
    return rc;
}

static int h_fill(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp;
    if (rsp_len)
    {
        *rsp_len = 0;
    }
    uint32_t idx = 0;
    char ab[128];
    if (copy_cstr_local(ab, sizeof(ab), req, req_len, &idx) != 0)
    {
        return -1;
    }
    if ((idx + 8) > req_len)
    {
        return -1;
    }
    uint32_t size = le32(&req[idx]);
    idx += 4;
    uint32_t seed = le32(&req[idx]);
    idx += 4;
    activity_led_pulse();
    return sd_fill_pattern(ab, size, seed);
}

static int h_status(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req;
    (void)req_len;
    if (!rsp || !rsp_len || *rsp_len < 4)
    {
        return -1;
    }
    int rc = sd_status();
    wr_le32(rsp, (uint32_t)rc);
    *rsp_len = 4;
    return 0;
}

static int h_peek(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    if (!rsp || !rsp_len)
    {
        return -1;
    }
    uint32_t idx = 0;
    char ab[128];
    if (copy_cstr_local(ab, sizeof(ab), req, req_len, &idx) != 0)
    {
        return -1;
    }
    if ((idx + 8) > req_len)
    {
        return -1;
    }
    uint32_t offset = le32(&req[idx]);
    idx += 4;
    uint32_t len = le32(&req[idx]);
    idx += 4;
    uint32_t cap = *rsp_len;
    if (len < cap)
    {
        cap = len;
    }
    uint32_t sc = cap;
    if (sc > sizeof(sd_dma_buf))
    {
        sc = sizeof(sd_dma_buf);
    }
    int rc = sd_read(ab, offset, sd_dma_buf, &sc);
    if (rc == 0)
    {
        memcpy(rsp, sd_dma_buf, sc);
        *rsp_len = sc;
    }
    return rc;
}

#ifdef __ZEPHYR__
#include <zephyr/storage/disk_access.h>
static int h_rawread(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    if (!rsp || !rsp_len || req_len < 8)
    {
        return -1;
    }
    uint32_t lba = le32(&req[0]);
    uint32_t cnt = le32(&req[4]);
    if (cnt == 0 || cnt > 1)
    {
        cnt = 1;
    }
    uint32_t bytes = 512u * cnt;
    if (*rsp_len < bytes)
    {
        bytes = *rsp_len;
    }
    uint32_t sc = bytes;
    if (sc > sizeof(sd_dma_buf))
    {
        sc = sizeof(sd_dma_buf);
    }
    int rc = disk_access_read("SD", sd_dma_buf, lba, cnt);
    /* quiet */
    if (rc == 0)
    {
        memcpy(rsp, sd_dma_buf, sc);
        *rsp_len = sc;
    }
    return rc;
}
#endif

int cmd_sd_init(void)
{
    int rc = 0;
    if (cmd_register(CMD_ID_SD_FORMAT, h_format) != 0)
    {
        rc = -1;
    }
    if (cmd_register(CMD_ID_SD_LIST, h_list) != 0)
    {
        rc = -1;
    }
    if (cmd_register(CMD_ID_SD_READ, h_read) != 0)
    {
        rc = -1;
    }
    if (cmd_register(CMD_ID_SD_WRITE, h_write) != 0)
    {
        rc = -1;
    }
    if (cmd_register(CMD_ID_SD_RENAME, h_rename) != 0)
    {
        rc = -1;
    }
    if (cmd_register(CMD_ID_SD_DELETE, h_delete) != 0)
    {
        rc = -1;
    }
    if (cmd_register(CMD_ID_SD_MKDIR, h_mkdir) != 0)
    {
        rc = -1;
    }
    if (cmd_register(CMD_ID_SD_STAT, h_stat) != 0)
    {
        rc = -1;
    }
    if (cmd_register(CMD_ID_SD_CHECKSUM, h_checksum) != 0)
    {
        rc = -1;
    }
    if (cmd_register(CMD_ID_SD_STATUS, h_status) != 0)
    {
        rc = -1;
    }
    if (cmd_register(CMD_ID_SD_PEEK, h_peek) != 0)
    {
        rc = -1;
    }
#ifdef __ZEPHYR__
    if (cmd_register(CMD_ID_SD_RAWREAD, h_rawread) != 0)
    {
        rc = -1;
    }
#endif
    if (cmd_register(CMD_ID_SD_FILL, h_fill) != 0)
    {
        rc = -1;
    }
    return rc;
}
