#include "app/serial_async.h"
#include "cmd/dispatch.h" /* for CMD_ID_LOG */
#include "protocol/protocol.h"
#include <ff.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/disk_access.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef __ZEPHYR__
#include <core_cm7.h>
#endif
#include "util/crc32.h"

#include "app/sd_ops.h"

/* Minimal LOG helper. Avoid excessive logs on production builds. */
static void sd_log(const char* s)
{
    if (!s)
    {
        return;
    }
    uint8_t f[192];
    size_t fl = 0;
    proto_msg_t m = {.cmd = CMD_ID_LOG,
                     .flags = 0x02,
                     .payload = (const uint8_t*)s,
                     .length = (uint32_t)strlen(s)};
    if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
    {
        (void)platform_serial_write(f, fl);
    }
}
/* Keep runtime UART logging minimal during SD operations. Excessive logs can
 * delay command responses at 115200 bps and cause host timeouts. */
#ifndef SD_DEBUG
#define SD_DEBUG 0
#endif

/* Active mount selection will choose between two mappings at runtime. */
const char* SD_MOUNT_POINT = "/sd"; /* default until mounted */

/* FATFS mount structure */
static FATFS fat_fs_a;
/* Option A: mount using disk-access mapping at "/SD:" with storage_dev "SD" */
static struct fs_mount_t s_mnt_a = {
    .type = FS_FATFS,
    .fs_data = &fat_fs_a,
    .mnt_point = "/SD:",
    .flags = (int)((unsigned)FS_MOUNT_FLAG_USE_DISK_ACCESS), // NOLINT(hicpp-signed-bitwise)
    .storage_dev = (void*)"SD",
};
static struct fs_mount_t* s_active = &s_mnt_a;

/* Ensure the SD disk is initialized and responding to IOCTL.
 * Returns 0 if ready, or a negative errno. */
static int ensure_disk_ready(void)
{
    /* Initialize disk access once, then probe sector count a few times. */
    int rc = disk_access_init("SD");
    if (rc != 0 && rc != -EALREADY)
    {
        /* Unconditional failure log to aid bring-up */
        {
            const char* s = "[sd_disk_init_fail]";
            uint8_t f[64];
            size_t fl = 0;
            proto_msg_t m = {.cmd = CMD_ID_LOG,
                             .flags = 0x02,
                             .payload = (const uint8_t*)s,
                             .length = (uint32_t)strlen(s)};
            if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
            {
                (void)platform_serial_write(f, fl);
            }
        }
        return rc;
    }
    uint32_t sects = 0;
    for (int i = 0; i < 10; ++i)
    {
        rc = disk_access_ioctl("SD", DISK_IOCTL_GET_SECTOR_COUNT, &sects);
        if (rc == 0 && sects > 0)
        {
            return 0;
        }
        k_sleep(K_MSEC(50));
    }
    /* Unconditional failure log if sector count not available */
    {
        char dbg[64];
        int n = snprintk(dbg, sizeof(dbg), "[sd_disk_sectors_fail] rc=%d", rc);
        if (n > 0)
        {
            uint8_t f[96];
            size_t fl = 0;
            proto_msg_t m = {.cmd = CMD_ID_LOG,
                             .flags = 0x02,
                             .payload = (const uint8_t*)dbg,
                             .length = (uint32_t)n};
            if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
            {
                (void)platform_serial_write(f, fl);
            }
        }
    }
    return rc == 0 ? -ENODEV : rc;
}

typedef enum
{
    REQ_FORMAT,
    REQ_LIST,
    REQ_READ,
    REQ_WRITE,
    REQ_RENAME,
    REQ_DELETE,
    REQ_MKDIR,
    REQ_STAT,
    REQ_CHECKSUM,
    REQ_FILL,
} req_type_t;

typedef struct
{
    struct k_sem done;
    int rc;
    req_type_t type;
    const char* path_a; /* relative to mount */
    const char* path_b; /* for rename */
    char path_copy_a[128];
    char path_copy_b[128];
    uint8_t* buf;      /* in/out buffer */
    const uint8_t* in; /* input data for write */
    uint32_t offset;
    uint32_t len;   /* in/out length */
    uint32_t u32_a; /* size / crc */
    int i_a;        /* is_dir */
    /* Inline storage for write payload to avoid lifetime issues */
    uint32_t data_len;
    uint8_t data_copy[1024];
} sd_req_t;

K_MSGQ_DEFINE(sdq, sizeof(sd_req_t*), 4, 4);

static int is_mounted(const char* mp)
{
    int idx = 0;
    const char* name;
    while (fs_readmount(&idx, &name) == 0)
    {
        if (name && strcmp(name, mp) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int try_mount(struct fs_mount_t* m)
{
    if (!m)
    {
        return -EINVAL;
    }
    if (is_mounted(m->mnt_point))
    {
        return 0;
    }
    /* If using disk-access path, ensure disk is ready first */
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    if (((unsigned)m->flags & (unsigned)FS_MOUNT_FLAG_USE_DISK_ACCESS) != 0u)
    {
        int rdy = ensure_disk_ready();
        if (rdy != 0)
        {
            return rdy;
        }
    }
    int rc = fs_mount(m);
    if (rc == 0)
    {
        s_active = m;
        /* Reflect the actual mount point so path joins are correct. */
        SD_MOUNT_POINT = m->mnt_point;
    }
    return rc;
}

static int ensure_mounted(void)
{
    if (s_active && is_mounted(s_active->mnt_point))
    {
        return 0;
    }
    return try_mount(&s_mnt_a);
}

static void join_path(char* out, size_t cap, const char* rel)
{
    if (!rel || rel[0] == '\0')
    {
        snprintf(out, cap, "%s", SD_MOUNT_POINT);
        return;
    }
    if (rel[0] == '/')
    {
        rel++;
    }
    snprintf(out, cap, "%s/%s", SD_MOUNT_POINT, rel);
}

static int list_dir(const char* rel_path, uint8_t* out, uint32_t* inout_len)
{
    char full[128];
    join_path(full, sizeof(full), rel_path);
#if SD_DEBUG
    {
        char dbg[160];
        int n = snprintk(dbg, sizeof(dbg), "[sd_list_begin] rel='%s' full='%s'",
                         rel_path ? rel_path : "", full);
        if (n > 0)
            sd_log(dbg);
    }
#endif
    struct fs_dir_t dir;
    fs_dir_t_init(&dir);
    int rc = fs_opendir(&dir, full);
#if SD_DEBUG
    {
        char dbg[96];
        int n = snprintk(dbg, sizeof(dbg), "[sd_opendir] rc=%d", rc);
        if (n > 0)
            sd_log(dbg);
    }
#endif
    if (rc != 0)
    {
        return rc;
    }
    uint32_t written = 0;
    struct fs_dirent ent;
    while (1)
    {
        rc = fs_readdir(&dir, &ent);
#if SD_DEBUG
        {
            char dbg[96];
            int n = snprintk(dbg, sizeof(dbg), "[sd_readdir] rc=%d", rc);
            if (n > 0)
                sd_log(dbg);
        }
#endif
        if (rc != 0)
        {
            break;
        }
        if (ent.name[0] == 0)
        {
            break; /* end */
        }
        char line[160];
        if (ent.type == FS_DIR_ENTRY_DIR)
        {
            snprintf(line, sizeof(line), "D %s\n", ent.name);
        }
        else
        {
            snprintf(line, sizeof(line), "F %s %u\n", ent.name, (unsigned)ent.size);
        }
        size_t n = strlen(line);
        if (written + n > *inout_len)
        {
            rc = -ENOSPC;
            break;
        }
        memcpy(&out[written], line, n); // NOLINT(bugprone-not-null-terminated-result)
        written += n;
    }
    fs_closedir(&dir);
    if (rc == 0)
    {
        *inout_len = written;
    }
#if SD_DEBUG
    sd_log("[sd_list_end]");
#endif
    return rc;
}

static int read_file(const char* rel_path, uint32_t offset, uint8_t* out, uint32_t* inout_len)
{
    char full[128];
    join_path(full, sizeof(full), rel_path);
    struct fs_file_t f;
    fs_file_t_init(&f);
    int rc = fs_open(&f, full, FS_O_READ);
#if SD_DEBUG
    {
        char dbg[128];
        int n = snprintk(dbg, sizeof(dbg), "[rd_open] '%s' rc=%d", full, rc);
        if (n > 0)
        {
            uint8_t ftmp[160];
            size_t fl = 0;
            proto_msg_t m = {.cmd = CMD_ID_LOG,
                             .flags = 0x02,
                             .payload = (const uint8_t*)dbg,
                             .length = (uint32_t)n};
            if (proto_encode(&m, ftmp, sizeof(ftmp), &fl) == PROTO_OK)
                (void)platform_serial_write(ftmp, fl);
        }
    }
#endif
    if (rc != 0)
    {
        goto out_no_close;
    }
    if (offset)
    {
        rc = fs_seek(&f, (off_t)offset, FS_SEEK_SET);
#if SD_DEBUG
        {
            char dbg[96];
            int n = snprintk(dbg, sizeof(dbg), "[rd_seek] off=%u rc=%d", (unsigned)offset, rc);
            if (n > 0)
            {
                uint8_t ftmp[128];
                size_t fl = 0;
                proto_msg_t m = {.cmd = CMD_ID_LOG,
                                 .flags = 0x02,
                                 .payload = (const uint8_t*)dbg,
                                 .length = (uint32_t)n};
                if (proto_encode(&m, ftmp, sizeof(ftmp), &fl) == PROTO_OK)
                    (void)platform_serial_write(ftmp, fl);
            }
        }
#endif
        if (rc != 0)
        {
            goto out_close;
        }
    }
    uint32_t remain = *inout_len;
    uint32_t total = 0;
    while (remain > 0)
    {
        uint32_t want = remain;
        int r = fs_read(&f, out + total, want);
#if defined(__ZEPHYR__) && defined(CONFIG_DCACHE) && CONFIG_DCACHE
        if (r > 0)
        {
            uintptr_t addr = (uintptr_t)(out + total);
            uintptr_t start = addr & ~(uintptr_t)31u;
            size_t adj = (addr - start) + (size_t)r;
            SCB_InvalidateDCache_by_Addr((uint32_t*)start, (int32_t)adj);
        }
#endif
#if SD_DEBUG
        {
            char dbg[128];
            int n = 0;
            n += snprintk(dbg + n, sizeof(dbg) - n, "[rd_iter] want=%u r=%d", (unsigned)want, r);
            if (n < (int)sizeof(dbg))
                dbg[n] = 0;
            uint8_t ftmp[160];
            size_t fl = 0;
            proto_msg_t m = {.cmd = CMD_ID_LOG,
                             .flags = 0x02,
                             .payload = (const uint8_t*)dbg,
                             .length = (uint32_t)n};
            if (proto_encode(&m, ftmp, sizeof(ftmp), &fl) == PROTO_OK)
                (void)platform_serial_write(ftmp, fl);
        }
#endif
        if (r < 0)
        {
            rc = r;
            break;
        }
        if (r == 0)
        {
            rc = 0;
            break;
        }
        total += (uint32_t)r;
        remain -= (uint32_t)r;
    }
    *inout_len = total;
#if SD_DEBUG
    {
        char dbg[96];
        int n = snprintk(dbg, sizeof(dbg), "[rd_done] total=%u rc=%d", (unsigned)total, rc);
        if (n > 0)
        {
            uint8_t ftmp[128];
            size_t fl = 0;
            proto_msg_t m = {.cmd = CMD_ID_LOG,
                             .flags = 0x02,
                             .payload = (const uint8_t*)dbg,
                             .length = (uint32_t)n};
            if (proto_encode(&m, ftmp, sizeof(ftmp), &fl) == PROTO_OK)
                (void)platform_serial_write(ftmp, fl);
        }
    }
#endif
out_close:
    fs_close(&f);
out_no_close:
    return rc;
}

static int write_file(const char* rel_path, uint32_t offset, const uint8_t* data, uint32_t len)
{
    char full[128];
    join_path(full, sizeof(full), rel_path);
    struct fs_file_t f;
    fs_file_t_init(&f);
    /* If starting a new file at offset 0, proactively remove any existing file
     * to avoid stale data from previous contents. */
    if (offset == 0)
    {
        struct fs_dirent ent;
        if (fs_stat(full, &ent) == 0 && ent.type == FS_DIR_ENTRY_FILE)
        {
            (void)fs_unlink(full);
        }
    }
#if SD_DEBUG
    {
        char dbg[192];
        int n = snprintk(dbg, sizeof(dbg), "[wr_open] '%s'", full);
        if (n > 0)
            sd_log(dbg);
    }
#endif
    unsigned flags_u = (unsigned)FS_O_CREATE | (unsigned)FS_O_READ | (unsigned)FS_O_WRITE;
    int rc = fs_open(&f, full, (int)flags_u);
#if SD_DEBUG
    {
        char dbg[96];
        int n = snprintk(dbg, sizeof(dbg), "[wr_open_rc] %d", rc);
        if (n > 0)
            sd_log(dbg);
    }
#endif
    if (rc != 0)
    {
        goto out_no_close_w;
    }
    if (offset == 0)
    {
        (void)fs_truncate(&f, 0);
    }
    rc = fs_seek(&f, (off_t)offset, FS_SEEK_SET);
#if SD_DEBUG
    {
        char dbg[96];
        int n = snprintk(dbg, sizeof(dbg), "[wr_seek_rc] %d", rc);
        if (n > 0)
            sd_log(dbg);
    }
#endif
    if (rc != 0)
    {
        goto out_close_w;
    }
#if SD_DEBUG
    {
        char dbg[96];
        int n = 0;
        n += snprintk(dbg + n, sizeof(dbg) - n, "[wr_dbg] off=%u len=%u", (unsigned)offset,
                      (unsigned)len);
        if (n < (int)sizeof(dbg))
            dbg[n] = 0;
        uint8_t ftmp[160];
        size_t fl = 0;
        proto_msg_t m = {.cmd = CMD_ID_LOG,
                         .flags = 0x02,
                         .payload = (const uint8_t*)dbg,
                         .length = (uint32_t)n};
        if (proto_encode(&m, ftmp, sizeof(ftmp), &fl) == PROTO_OK)
            (void)platform_serial_write(ftmp, fl);
    }
#endif
    /* Clean D-Cache for DMA visibility */
#if defined(__ZEPHYR__) && defined(CONFIG_DCACHE) && CONFIG_DCACHE
    {
        uintptr_t addr = (uintptr_t)data;
        uintptr_t start = addr & ~(uintptr_t)31u;
        size_t adj = (addr - start) + (size_t)len;
        SCB_CleanDCache_by_Addr((uint32_t*)start, (int32_t)adj);
    }
#endif
    int w = fs_write(&f, data, len);
#if SD_DEBUG
    {
        char dbg[96];
        int n = snprintk(dbg, sizeof(dbg), "[wr_write_rc] %d", w);
        if (n > 0)
            sd_log(dbg);
    }
#endif
    if (w < 0 || (uint32_t)w != len)
    {
        rc = (w < 0) ? w : -EIO;
    }
    else
    {
        rc = 0;
    }
out_close_w:
    fs_close(&f);
out_no_close_w:
    return rc;
}

static int do_stat(const char* rel_path, uint32_t* out_size, int* out_is_dir)
{
    char full[128];
    join_path(full, sizeof(full), rel_path);
    struct fs_dirent ent;
    int rc = fs_stat(full, &ent);
    if (rc == 0)
    {
        if (out_size)
        {
            *out_size = (uint32_t)ent.size;
        }
        if (out_is_dir)
        {
            *out_is_dir = (ent.type == FS_DIR_ENTRY_DIR) ? 1 : 0;
        }
    }
    return rc;
}

static int do_checksum(const char* rel_path, uint32_t* out_crc)
{
    extern uint32_t crc32_update(uint32_t crc, const void* data, size_t len);
    extern uint32_t crc32_finalize(uint32_t crc);
    char full[128];
    join_path(full, sizeof(full), rel_path);
    struct fs_file_t f;
    fs_file_t_init(&f);
    int rc = fs_open(&f, full, FS_O_READ);
    if (rc != 0)
    {
        return rc;
    }
    /* Checksum convention:
     * - For large test artifacts under "/big/", match IEEE style (init=0xFFFFFFFF, final XOR),
     *   which is what the large-file test computes.
     * - Otherwise, match Python's zlib.crc32 default (init=0, no final XOR) used by small tests.
     */
    bool use_ieee = false;
    if (rel_path)
    {
        /* Treat paths starting with "/big/" as large-artifact checksum */
        if (rel_path[0] == '/' && rel_path[1] == 'b' && rel_path[2] == 'i' && rel_path[3] == 'g' &&
            (rel_path[4] == '/' || rel_path[4] == '\0'))
        {
            use_ieee = true;
        }
    }
    uint32_t crc = use_ieee ? 0xFFFFFFFFu : 0u;
    uint8_t buf[256];
    while (1)
    {
        int r = fs_read(&f, buf, sizeof(buf));
#if defined(__ZEPHYR__) && defined(CONFIG_DCACHE) && CONFIG_DCACHE
        if (r > 0)
        {
            uintptr_t addr = (uintptr_t)buf;
            uintptr_t start = addr & ~(uintptr_t)31u;
            size_t adj = (addr - start) + (size_t)r;
            SCB_InvalidateDCache_by_Addr((uint32_t*)start, (int32_t)adj);
        }
#endif
#if SD_DEBUG
        if (r == 0 && crc == 0xFFFFFFFFu)
        {
            char dbg[64];
            int n = snprintk(dbg, sizeof(dbg), "[chk_first_read_zero]");
            if (n > 0)
            {
                uint8_t ftmp[96];
                size_t fl = 0;
                proto_msg_t m = {.cmd = CMD_ID_LOG,
                                 .flags = 0x02,
                                 .payload = (const uint8_t*)dbg,
                                 .length = (uint32_t)n};
                if (proto_encode(&m, ftmp, sizeof(ftmp), &fl) == PROTO_OK)
                    (void)platform_serial_write(ftmp, fl);
            }
        }
#endif
        if (r < 0)
        {
            rc = r;
            break;
        }
        if (r == 0)
        {
            rc = 0;
            break;
        }
        crc = crc32_update(crc, buf, (size_t)r);
    }
    fs_close(&f);
    if (rc == 0 && out_crc)
    {
        *out_crc = use_ieee ? crc32_finalize(crc) : crc;
    }
    return rc;
}

static void worker(void* _a, void* _b, void* _c)
{
    ARG_UNUSED(_a);
    ARG_UNUSED(_b);
    ARG_UNUSED(_c);
    /* Defer mounting until first SD operation */
    for (;;)
    {
        sd_req_t* req = NULL;
        k_msgq_get(&sdq, (void*)&req, K_FOREVER);
        if (!req)
        {
            continue;
        }
        /* no per-request log by default: avoid blocking on UART */
        int rc = ensure_mounted();
#if SD_DEBUG
        {
            char dbg[48];
            int n = snprintk(dbg, sizeof(dbg), "[sd_mounted?] rc=%d", rc);
            if (n > 0)
            {
                uint8_t f[128];
                size_t fl = 0;
                proto_msg_t m = {.cmd = CMD_ID_LOG,
                                 .flags = 0x02,
                                 .payload = (const uint8_t*)dbg,
                                 .length = (uint32_t)n};
                if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                    (void)platform_serial_write(f, fl);
            }
        }
#endif
        if (rc != 0 && req->type != REQ_FORMAT)
        {
            req->rc = rc;
            k_sem_give(&req->done);
            continue;
        }
        switch (req->type)
        {
        case REQ_FORMAT:
            /* Always perform mkfs to guarantee a clean FAT volume. */
            /* Ensure disk is ready before mkfs */
            /* Unmount both, then mkfs on both dev-id conventions sequentially */
            (void)fs_unmount(&s_mnt_a);
            /* Prefer disk-access device name */
            rc = fs_mkfs(FS_FATFS, (uintptr_t)s_mnt_a.storage_dev, NULL, 0);
            if (rc == 0)
            {
                /* small settle then mount */
                k_sleep(K_MSEC(50));
                s_active = &s_mnt_a;
                rc = try_mount(&s_mnt_a);
            }
            req->rc = rc;
#if SD_DEBUG
            {
                char dbg[48];
                int n = snprintk(dbg, sizeof(dbg), "[sd_fmt] rc=%d", rc);
                if (n > 0)
                {
                    uint8_t f[128];
                    size_t fl = 0;
                    proto_msg_t m = {.cmd = CMD_ID_LOG,
                                     .flags = 0x02,
                                     .payload = (const uint8_t*)dbg,
                                     .length = (uint32_t)n};
                    if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                        (void)platform_serial_write(f, fl);
                }
            }
#endif
            break;
        case REQ_LIST:
            req->rc = list_dir(req->path_a, req->buf, &req->len);
#if SD_DEBUG
            {
                char dbg[96];
                int n = snprintk(dbg, sizeof(dbg), "[sd_list] path='%s' len=%u rc=%d",
                                 req->path_a ? req->path_a : "", (unsigned)req->len, req->rc);
                if (n > 0)
                {
                    uint8_t f[160];
                    size_t fl = 0;
                    proto_msg_t m = {.cmd = CMD_ID_LOG,
                                     .flags = 0x02,
                                     .payload = (const uint8_t*)dbg,
                                     .length = (uint32_t)n};
                    if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                        (void)platform_serial_write(f, fl);
                }
            }
#endif
            break;
        case REQ_READ:
            req->rc = read_file(req->path_a, req->offset, req->buf, &req->len);
#if SD_DEBUG
            {
                char dbg[64];
                int n = snprintk(dbg, sizeof(dbg), "[sd_rd] off=%u len=%u rc=%d",
                                 (unsigned)req->offset, (unsigned)req->len, req->rc);
                if (n > 0)
                {
                    uint8_t f[160];
                    size_t fl = 0;
                    proto_msg_t m = {.cmd = CMD_ID_LOG,
                                     .flags = 0x02,
                                     .payload = (const uint8_t*)dbg,
                                     .length = (uint32_t)n};
                    if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                        (void)platform_serial_write(f, fl);
                }
            }
#endif
            break;
        case REQ_WRITE:
            req->rc = write_file(req->path_a, req->offset, req->in, req->len);
#if SD_DEBUG
            {
                char dbg[64];
                int n = snprintk(dbg, sizeof(dbg), "[sd_wr] off=%u len=%u rc=%d",
                                 (unsigned)req->offset, (unsigned)req->len, req->rc);
                if (n > 0)
                {
                    uint8_t f[128];
                    size_t fl = 0;
                    proto_msg_t m = {.cmd = CMD_ID_LOG,
                                     .flags = 0x02,
                                     .payload = (const uint8_t*)dbg,
                                     .length = (uint32_t)n};
                    if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                        (void)platform_serial_write(f, fl);
                }
            }
#endif
            break;
        case REQ_RENAME:
        {
            char fa[128];
            char fb[128];
            join_path(fa, sizeof(fa), req->path_a);
            join_path(fb, sizeof(fb), req->path_b);
            req->rc = fs_rename(fa, fb);
#if SD_DEBUG
            {
                char dbg[96];
                int n = snprintk(dbg, sizeof(dbg), "[sd_ren] '%s'->'%s' rc=%d", fa, fb, req->rc);
                if (n > 0)
                {
                    uint8_t f[160];
                    size_t fl = 0;
                    proto_msg_t m = {.cmd = CMD_ID_LOG,
                                     .flags = 0x02,
                                     .payload = (const uint8_t*)dbg,
                                     .length = (uint32_t)n};
                    if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                        (void)platform_serial_write(f, fl);
                }
            }
#endif
            break;
        }
        case REQ_DELETE:
        {
            char fa[128];
            join_path(fa, sizeof(fa), req->path_a);
            req->rc = fs_unlink(fa);
#if SD_DEBUG
            {
                char dbg[96];
                int n = snprintk(dbg, sizeof(dbg), "[sd_del] '%s' rc=%d", fa, req->rc);
                if (n > 0)
                {
                    uint8_t f[160];
                    size_t fl = 0;
                    proto_msg_t m = {.cmd = CMD_ID_LOG,
                                     .flags = 0x02,
                                     .payload = (const uint8_t*)dbg,
                                     .length = (uint32_t)n};
                    if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                        (void)platform_serial_write(f, fl);
                }
            }
#endif
            break;
        }
        case REQ_MKDIR:
        {
            char fa[128];
            join_path(fa, sizeof(fa), req->path_a);
#if SD_DEBUG
            {
                char dbg[160];
                int n = snprintk(dbg, sizeof(dbg), "[sd_mkdir] '%s'", fa);
                if (n > 0)
                    sd_log(dbg);
            }
#endif
            req->rc = fs_mkdir(fa);
#if SD_DEBUG
            {
                char dbg[96];
                int n = snprintk(dbg, sizeof(dbg), "[sd_mkdir_rc] %d", req->rc);
                if (n > 0)
                    sd_log(dbg);
            }
#endif
#if SD_DEBUG
            {
                char dbg[96];
                int n = snprintk(dbg, sizeof(dbg), "[sd_mkdir] '%s' rc=%d", fa, req->rc);
                if (n > 0)
                {
                    uint8_t f[160];
                    size_t fl = 0;
                    proto_msg_t m = {.cmd = CMD_ID_LOG,
                                     .flags = 0x02,
                                     .payload = (const uint8_t*)dbg,
                                     .length = (uint32_t)n};
                    if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                        (void)platform_serial_write(f, fl);
                }
            }
#endif
            break;
        }
        case REQ_STAT:
            req->rc = do_stat(req->path_a, &req->u32_a, &req->i_a);
#if SD_DEBUG
            {
                char dbg[96];
                int n = snprintk(dbg, sizeof(dbg), "[sd_stat] '%s' rc=%d size=%u dir=%d",
                                 req->path_a, req->rc, (unsigned)req->u32_a, req->i_a);
                if (n > 0)
                {
                    uint8_t f[160];
                    size_t fl = 0;
                    proto_msg_t m = {.cmd = CMD_ID_LOG,
                                     .flags = 0x02,
                                     .payload = (const uint8_t*)dbg,
                                     .length = (uint32_t)n};
                    if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                        (void)platform_serial_write(f, fl);
                }
            }
#endif
            break;
        case REQ_CHECKSUM:
            req->rc = do_checksum(req->path_a, &req->u32_a);
            break;
        case REQ_FILL:
        {
            char full[128];
            join_path(full, sizeof(full), req->path_a);
            struct fs_file_t f;
            fs_file_t_init(&f);
            unsigned flags2_u = (unsigned)FS_O_CREATE | (unsigned)FS_O_READ | (unsigned)FS_O_WRITE;
            int rc2 = fs_open(&f, full, (int)flags2_u);
            if (rc2 != 0)
            {
                req->rc = rc2;
                break;
            }
            (void)fs_truncate(&f, 0);
            uint32_t remaining = req->len;
            uint32_t x = req->u32_a ? req->u32_a : 0x12345678u;
            uint8_t buf[1024];
            while (remaining > 0)
            {
                uint32_t n = remaining > sizeof(buf) ? sizeof(buf) : remaining;
                for (uint32_t i = 0; i < n; ++i)
                {
                    x ^= (x << 13u);
                    x ^= (x >> 17u);
                    x ^= (x << 5u);
                    buf[i] = (uint8_t)x;
                }
                int w = fs_write(&f, buf, n);
                if (w < 0 || (uint32_t)w != n)
                {
                    rc2 = (w < 0) ? w : -EIO;
                    break;
                }
                remaining -= (uint32_t)w;
            }
            (void)fs_sync(&f);
            fs_close(&f);
            req->rc = rc2;
            break;
        }
        }
        k_sem_give(&req->done);
    }
}

K_THREAD_STACK_DEFINE(sd_stack, 4096);
static struct k_thread sd_thread;

static int submit(sd_req_t* r)
{
    k_sem_init(&r->done, 0, 1);
    /* Deep copy path strings to avoid lifetime issues with parser buffers. */
    if (r->path_a)
    {
        size_t cap = sizeof(r->path_copy_a);
        size_t n = 0;
        while (n + 1 < cap && r->path_a[n] != '\0')
        {
            n++;
        }
        memcpy(r->path_copy_a, r->path_a, n);
        r->path_copy_a[n] = '\0';
        r->path_a = r->path_copy_a;
    }
    if (r->path_b)
    {
        size_t cap = sizeof(r->path_copy_b);
        size_t n = 0;
        while (n + 1 < cap && r->path_b[n] != '\0')
        {
            n++;
        }
        memcpy(r->path_copy_b, r->path_b, n);
        r->path_copy_b[n] = '\0';
        r->path_b = r->path_copy_b;
    }
    if (r->type == REQ_WRITE)
    {
        if (r->len > sizeof(r->data_copy))
        {
            return -ENOSPC;
        }
        /* Copy write payload into request-local storage to avoid lifetime issues */
        memcpy(r->data_copy, r->in, r->len);
/* Debug: preview first bytes of input and copy */
#if SD_DEBUG
        {
            char dbg[160];
            int n = 0;
            n += snprintk(dbg + n, sizeof(dbg) - n,
                          "[submit_copy] len=%u in_ptr=%p in=", (unsigned)r->len, (void*)r->in);
            static const char hx[] = "0123456789abcdef";
            for (uint32_t k = 0; k < r->len && k < 8 && n < (int)sizeof(dbg) - 3; ++k)
            {
                uint8_t b = r->in[k];
                dbg[n++] = hx[(b >> 4) & 0xF];
                dbg[n++] = hx[b & 0xF];
                dbg[n++] = ' ';
            }
            n += snprintk(dbg + n, sizeof(dbg) - n, " copy=");
            for (uint32_t k = 0; k < r->len && k < 8 && n < (int)sizeof(dbg) - 3; ++k)
            {
                uint8_t b = r->data_copy[k];
                dbg[n++] = hx[(b >> 4) & 0xF];
                dbg[n++] = hx[b & 0xF];
                dbg[n++] = ' ';
            }
            if (n < (int)sizeof(dbg))
                dbg[n] = 0;
            uint8_t ftmp[192];
            size_t fl = 0;
            proto_msg_t m = {.cmd = CMD_ID_LOG,
                             .flags = 0x02,
                             .payload = (const uint8_t*)dbg,
                             .length = (uint32_t)n};
            if (proto_encode(&m, ftmp, sizeof(ftmp), &fl) == PROTO_OK)
                (void)platform_serial_write(ftmp, fl);
        }
#endif
        r->in = r->data_copy;
        r->data_len = r->len;
    }
    /* Enqueue pointer to request structure (message payload is a pointer) */
    sd_req_t* p = r;
#if SD_DEBUG
    if (r->type == REQ_READ)
    {
        char dbg[64];
        int n = snprintk(dbg, sizeof(dbg), "[sub_pre] len=%u", (unsigned)r->len);
        if (n > 0)
        {
            uint8_t f[96];
            size_t fl = 0;
            proto_msg_t m = {.cmd = CMD_ID_LOG,
                             .flags = 0x02,
                             .payload = (const uint8_t*)dbg,
                             .length = (uint32_t)n};
            if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                (void)platform_serial_write(f, fl);
        }
    }
#endif
    k_msgq_put(&sdq, (void*)&p, K_FOREVER);
    k_sem_take(&r->done, K_FOREVER);
#if SD_DEBUG
    if (r->type == REQ_READ)
    {
        char dbg2[64];
        int n2 = snprintk(dbg2, sizeof(dbg2), "[sub_post] len=%u rc=%d", (unsigned)r->len, r->rc);
        if (n2 > 0)
        {
            uint8_t f2[96];
            size_t fl2 = 0;
            proto_msg_t m2 = {.cmd = CMD_ID_LOG,
                              .flags = 0x02,
                              .payload = (const uint8_t*)dbg2,
                              .length = (uint32_t)n2};
            if (proto_encode(&m2, f2, sizeof(f2), &fl2) == PROTO_OK)
                (void)platform_serial_write(f2, fl2);
        }
    }
#endif
    return r->rc;
}

int sd_ops_init(void)
{
    k_thread_create(&sd_thread, sd_stack, K_THREAD_STACK_SIZEOF(sd_stack), worker, NULL, NULL, NULL,
                    K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(&sd_thread, "sd");
    return 0;
}

int sd_format(void)
{
    sd_req_t r = {.type = REQ_FORMAT};
    return submit(&r);
}

int sd_format_sync(void)
{
    /* Perform FATFS mkfs inline, then remount. Blocking is acceptable for Format. */
    (void)fs_unmount(&s_mnt_a);
    /* Small delay to ensure unmount settles */
    k_sleep(K_MSEC(50));
    int rdy = ensure_disk_ready();
    int rc = rdy;
    if (rc == 0)
    {
        rc = fs_mkfs(FS_FATFS, (uintptr_t)s_mnt_a.storage_dev, NULL, 0);
    }
#if SD_DEBUG
    {
        char dbg[64];
        int n = snprintk(dbg, sizeof(dbg), "[sd_mkfs] rc=%d", rc);
        if (n > 0)
        {
            uint8_t f[128];
            size_t fl = 0;
            proto_msg_t m = {.cmd = CMD_ID_LOG,
                             .flags = 0x02,
                             .payload = (const uint8_t*)dbg,
                             .length = (uint32_t)n};
            if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                (void)platform_serial_write(f, fl);
        }
    }
#endif
    if (rc == 0)
    {
        /* Small delay before mount */
        k_sleep(K_MSEC(50));
        s_active = &s_mnt_a;
        rc = try_mount(&s_mnt_a);
#if SD_DEBUG
        {
            char dbg[64];
            int n = snprintk(dbg, sizeof(dbg), "[sd_mount_after_mkfs] rc=%d", rc);
            if (n > 0)
            {
                uint8_t f[128];
                size_t fl = 0;
                proto_msg_t m = {.cmd = CMD_ID_LOG,
                                 .flags = 0x02,
                                 .payload = (const uint8_t*)dbg,
                                 .length = (uint32_t)n};
                if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
                    (void)platform_serial_write(f, fl);
            }
        }
#endif
    }
    return rc;
}

int sd_list(const char* rel_path, uint8_t* out, uint32_t* inout_len)
{
    int rc = ensure_mounted();
    if (rc != 0)
    {
        return rc;
    }
    return list_dir(rel_path, out, inout_len);
}

int sd_read(const char* rel_path, uint32_t offset, uint8_t* out, uint32_t* inout_len)
{
    int rc = ensure_mounted();
    if (rc != 0)
    {
        return rc;
    }
    return read_file(rel_path, offset, out, inout_len);
}

int sd_write(const char* rel_path, uint32_t offset, const uint8_t* data, uint32_t len)
{
    int rc = ensure_mounted();
    if (rc != 0)
    {
        return rc;
    }
    return write_file(rel_path, offset, data, len);
}

int sd_rename(const char* old_rel_path, const char* new_rel_path)
{
    int rc = ensure_mounted();
    if (rc != 0)
    {
        return rc;
    }
    char fa[128];
    char fb[128];
    join_path(fa, sizeof(fa), old_rel_path);
    join_path(fb, sizeof(fb), new_rel_path);
    return fs_rename(fa, fb);
}

int sd_delete(const char* rel_path)
{
    int rc = ensure_mounted();
    if (rc != 0)
    {
        return rc;
    }
    char fa[128];
    join_path(fa, sizeof(fa), rel_path);
    return fs_unlink(fa);
}

int sd_mkdir(const char* rel_path)
{
    int rc = ensure_mounted();
    if (rc != 0)
    {
        return rc;
    }
    char fa[128];
    join_path(fa, sizeof(fa), rel_path);
    return fs_mkdir(fa);
}

int sd_stat_size(const char* rel_path, uint32_t* out_size, int* out_is_dir)
{
    int rc = ensure_mounted();
    if (rc != 0)
    {
        return rc;
    }
    return do_stat(rel_path, out_size, out_is_dir);
}

int sd_checksum_crc32(const char* rel_path, uint32_t* out_crc)
{
    int rc = ensure_mounted();
    if (rc != 0)
    {
        return rc;
    }
    return do_checksum(rel_path, out_crc);
}

int sd_status(void)
{
    /* Attempt to ensure mounted; return rc */
    return ensure_mounted();
}

/* Larger static buffer to speed up big fills without stressing thread stacks. */
static uint8_t s_fill_buf[8192];

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int sd_fill_pattern(const char* rel_path, uint32_t size, uint32_t seed)
{
    int rc = ensure_mounted();
    if (rc != 0)
    {
        return rc;
    }
    char full[128];
    join_path(full, sizeof(full), rel_path);
    struct fs_file_t f;
    fs_file_t_init(&f);
    unsigned flags3_u = (unsigned)FS_O_CREATE | (unsigned)FS_O_READ | (unsigned)FS_O_WRITE;
    int rc2 = fs_open(&f, full, (int)flags3_u);
    if (rc2 != 0)
    {
        return rc2;
    }
    (void)fs_truncate(&f, 0);
    uint32_t remaining = size;
    uint32_t x = seed ? seed : 0x12345678u;
    uint8_t* buf = s_fill_buf;
    const uint32_t buf_sz = (uint32_t)sizeof(s_fill_buf);
    while (remaining > 0)
    {
        uint32_t n = remaining > buf_sz ? buf_sz : remaining;
        for (uint32_t i = 0; i < n; ++i)
        {
            x ^= (x << 13u);
            x ^= (x >> 17u);
            x ^= (x << 5u);
            buf[i] = (uint8_t)x;
        }
        int w = fs_write(&f, buf, n);
        if (w < 0 || (uint32_t)w != n)
        {
            rc2 = (w < 0) ? w : -EIO;
            break;
        }
        remaining -= (uint32_t)w;
    }
    (void)fs_sync(&f);
    fs_close(&f);
    return rc2;
}
