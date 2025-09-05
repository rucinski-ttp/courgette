#include <string.h>
#include <zephyr/kernel.h>

#include "cmd/dispatch.h"
#include "app/sd_ops.h"
#include "app/activity_led.h"

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
 */

static const char* parse_cstr(const uint8_t* p, uint32_t n, uint32_t* idx)
{
    uint32_t i = *idx;
    const char* s = (const char*)&p[i];
    while (i < n && p[i] != '\0') i++;
    if (i >= n) return NULL;
    *idx = i + 1;
    return s;
}

static uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void wr_le32(uint8_t* p, uint32_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF; }

static int h_format(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req;(void)req_len;(void)rsp;(void)rsp_len;
    activity_led_pulse();
    return sd_format();
}

static int h_list(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    uint32_t idx = 0;
    const char* path = parse_cstr(req, req_len, &idx);
    if (!path || !rsp || !rsp_len) return -1;
    uint32_t cap = *rsp_len;
    int rc = sd_list(path, rsp, &cap);
    if (rc == 0) *rsp_len = cap;
    return rc;
}

static int h_read(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    uint32_t idx = 0;
    const char* path = parse_cstr(req, req_len, &idx);
    if (!path || (idx + 8) > req_len || !rsp || !rsp_len) return -1;
    uint32_t offset = le32(&req[idx]); idx += 4;
    uint32_t len = le32(&req[idx]); idx += 4;
    uint32_t cap = *rsp_len;
    if (len < cap) cap = len;
    int rc = sd_read(path, offset, rsp, &cap);
    if (rc == 0) *rsp_len = cap;
    return rc;
}

static int h_write(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp; if (rsp_len) *rsp_len = 0;
    uint32_t idx = 0;
    const char* path = parse_cstr(req, req_len, &idx);
    if (!path || (idx + 8) > req_len) return -1;
    uint32_t offset = le32(&req[idx]); idx += 4;
    uint32_t len = le32(&req[idx]); idx += 4;
    if (idx + len > req_len) return -1;
    activity_led_pulse();
    return sd_write(path, offset, &req[idx], len);
}

static int h_rename(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp; if (rsp_len) *rsp_len = 0;
    uint32_t idx = 0;
    const char* a = parse_cstr(req, req_len, &idx);
    const char* b = parse_cstr(req, req_len, &idx);
    if (!a || !b) return -1;
    return sd_rename(a, b);
}

static int h_delete(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp; if (rsp_len) *rsp_len = 0;
    uint32_t idx = 0; const char* a = parse_cstr(req, req_len, &idx);
    if (!a) return -1; return sd_delete(a);
}

static int h_mkdir(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp; if (rsp_len) *rsp_len = 0;
    uint32_t idx = 0; const char* a = parse_cstr(req, req_len, &idx);
    if (!a) return -1; return sd_mkdir(a);
}

static int h_stat(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    if (!rsp || !rsp_len || *rsp_len < 8) return -1;
    uint32_t idx = 0; const char* a = parse_cstr(req, req_len, &idx);
    if (!a) return -1;
    uint32_t size = 0; int is_dir = 0;
    int rc = sd_stat_size(a, &size, &is_dir);
    if (rc == 0) {
        wr_le32(&rsp[0], size);
        wr_le32(&rsp[4], (uint32_t)(is_dir ? 1 : 0));
        *rsp_len = 8;
    }
    return rc;
}

static int h_checksum(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    if (!rsp || !rsp_len || *rsp_len < 4) return -1;
    uint32_t idx = 0; const char* a = parse_cstr(req, req_len, &idx);
    if (!a) return -1;
    uint32_t crc = 0; int rc = sd_checksum_crc32(a, &crc);
    if (rc == 0) { wr_le32(rsp, crc); *rsp_len = 4; }
    return rc;
}

static int h_status(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req; (void)req_len;
    if (!rsp || !rsp_len || *rsp_len < 4) return -1;
    int rc = sd_status();
    wr_le32(rsp, (uint32_t)rc);
    *rsp_len = 4;
    return 0;
}

int cmd_sd_init(void)
{
    int rc = 0;
    rc |= cmd_register(CMD_ID_SD_FORMAT, h_format);
    rc |= cmd_register(CMD_ID_SD_LIST, h_list);
    rc |= cmd_register(CMD_ID_SD_READ, h_read);
    rc |= cmd_register(CMD_ID_SD_WRITE, h_write);
    rc |= cmd_register(CMD_ID_SD_RENAME, h_rename);
    rc |= cmd_register(CMD_ID_SD_DELETE, h_delete);
    rc |= cmd_register(CMD_ID_SD_MKDIR, h_mkdir);
    rc |= cmd_register(CMD_ID_SD_STAT, h_stat);
    rc |= cmd_register(CMD_ID_SD_CHECKSUM, h_checksum);
    rc |= cmd_register(CMD_ID_SD_STATUS, h_status);
    return rc;
}
