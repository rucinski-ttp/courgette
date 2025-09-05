#include <string.h>
#include <stdint.h>

#include "cmd/dispatch.h"

/* Simple and conservative memory access helpers.
 * Read:  req = [u32 addr][u32 len]
 *        rsp = [len bytes]
 * Write: req = [u32 addr][u32 len][len bytes]
 *        rsp = none (zero-length)
 * Limits: len <= 256. Writes restricted to SRAM/peripheral regions; flash writes are rejected.
 */

static int in_range(uint32_t addr, uint32_t len, uint32_t start, uint32_t end)
{
    if (len == 0) return 1;
    if (addr < start) return 0;
    if (addr > end) return 0;
    uint32_t last = addr + (len - 1u);
    if (last < addr) return 0; /* overflow */
    return last <= end;
}

static int is_flash(uint32_t addr, uint32_t len)
{
    return in_range(addr, len, 0x08000000u, 0x080FFFFFu);
}

static int is_sram(uint32_t addr, uint32_t len)
{
    /* STM32H747I-DISCO M7 SRAM regions (AXI SRAM at 0x24000000 .. 0x2407FFFF for 512KB) */
    if (in_range(addr, len, 0x20000000u, 0x2003FFFFu)) return 1; /* DTCM SRAM */
    if (in_range(addr, len, 0x24000000u, 0x2407FFFFu)) return 1; /* AXI SRAM */
    return 0;
}

static int handle_mem_read(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    if (!req || req_len < 8u || !rsp || !rsp_len) return -1;
    uint32_t addr = (uint32_t)req[0] | ((uint32_t)req[1] << 8) | ((uint32_t)req[2] << 16) |
                    ((uint32_t)req[3] << 24);
    uint32_t len = (uint32_t)req[4] | ((uint32_t)req[5] << 8) | ((uint32_t)req[6] << 16) |
                   ((uint32_t)req[7] << 24);
    if (len > 256u) return -1;
    if (*rsp_len < len) return -1;
    /* Allow read from flash or SRAM */
    if (!(is_flash(addr, len) || is_sram(addr, len))) return -1;
    const volatile uint8_t* p = (const volatile uint8_t*)addr;
    for (uint32_t i = 0; i < len; ++i) {
        rsp[i] = p[i];
    }
    *rsp_len = len;
    return 0;
}

static int handle_mem_write(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)rsp; (void)rsp_len;
    if (!req || req_len < 8u) return -1;
    uint32_t addr = (uint32_t)req[0] | ((uint32_t)req[1] << 8) | ((uint32_t)req[2] << 16) |
                    ((uint32_t)req[3] << 24);
    uint32_t len = (uint32_t)req[4] | ((uint32_t)req[5] << 8) | ((uint32_t)req[6] << 16) |
                   ((uint32_t)req[7] << 24);
    if (len > 256u) return -1;
    if ((8u + len) > req_len) return -1;
    /* Disallow writes to flash; permit writes to SRAM only */
    if (!is_sram(addr, len) || is_flash(addr, len)) return -1;
    volatile uint8_t* p = (volatile uint8_t*)addr;
    for (uint32_t i = 0; i < len; ++i) {
        p[i] = req[8 + i];
    }
    if (rsp_len) *rsp_len = 0;
    return 0;
}

int cmd_mem_init(void)
{
    int rc = 0;
    rc |= cmd_register(CMD_ID_MEM_READ, handle_mem_read);
    rc |= cmd_register(CMD_ID_MEM_WRITE, handle_mem_write);
    return rc;
}

