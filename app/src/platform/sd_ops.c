#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <ff.h>
#include "protocol/protocol.h"
#include "cmd/dispatch.h" /* for CMD_ID_LOG */
#include "app/serial_async.h"

#include <string.h>
#include <stdio.h>
#include "util/crc32.h"

#include "app/sd_ops.h"

/* Mount point */
const char* SD_MOUNT_POINT = "/sd";

/* FATFS mount structure */
static FATFS fat_fs;
static struct fs_mount_t s_mnt = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
    .mnt_point = "/sd",
    .flags = 0,
    .storage_dev = (void*)"SD:",
};

typedef enum {
    REQ_FORMAT,
    REQ_LIST,
    REQ_READ,
    REQ_WRITE,
    REQ_RENAME,
    REQ_DELETE,
    REQ_MKDIR,
    REQ_STAT,
    REQ_CHECKSUM,
} req_type_t;

typedef struct {
    struct k_sem done;
    int rc;
    req_type_t type;
    const char* path_a; /* relative to mount */
    const char* path_b; /* for rename */
    uint8_t* buf;       /* in/out buffer */
    const uint8_t* in;  /* input data for write */
    uint32_t offset;
    uint32_t len;       /* in/out length */
    uint32_t u32_a;     /* size / crc */
    int i_a;            /* is_dir */
} sd_req_t;

K_MSGQ_DEFINE(sdq, sizeof(sd_req_t*), 4, 4);

static int ensure_mounted(void)
{
    /* Check if already mounted by trying to read mount list */
    int idx = 0; const char* name;
    while (fs_readmount(&idx, &name) == 0) {
        if (name && strcmp(name, s_mnt.mnt_point) == 0) return 0;
        idx++;
    }
    /* Try mount */
    int rc = fs_mount(&s_mnt);
    {
        char dbg[48]; int n = snprintk(dbg, sizeof(dbg), "[sd_mount] rc=%d", rc);
        if (n > 0) { uint8_t f[128]; size_t fl=0; proto_msg_t m={.cmd=CMD_ID_LOG,.flags=0x02,.payload=(const uint8_t*)dbg,.length=(uint32_t)n}; if (proto_encode(&m,f,sizeof(f),&fl)==PROTO_OK) (void)platform_serial_write(f,fl);}    
    }
    return rc;
}

static void join_path(char* out, size_t cap, const char* rel)
{
    if (!rel || rel[0] == '\0') {
        snprintf(out, cap, "%s", SD_MOUNT_POINT);
        return;
    }
    if (rel[0] == '/') rel++;
    snprintf(out, cap, "%s/%s", SD_MOUNT_POINT, rel);
}

static int list_dir(const char* rel_path, uint8_t* out, uint32_t* inout_len)
{
    char full[128];
    join_path(full, sizeof(full), rel_path);
    struct fs_dir_t dir; fs_dir_t_init(&dir);
    int rc = fs_opendir(&dir, full);
    if (rc != 0) return rc;
    uint32_t written = 0;
    struct fs_dirent ent;
    while (1) {
        rc = fs_readdir(&dir, &ent);
        if (rc != 0) break;
        if (ent.name[0] == 0) break; /* end */
        char line[160];
        if (ent.type == FS_DIR_ENTRY_DIR) {
            snprintf(line, sizeof(line), "D %s\n", ent.name);
        } else {
            snprintf(line, sizeof(line), "F %s %u\n", ent.name, (unsigned)ent.size);
        }
        size_t n = strlen(line);
        if (written + n > *inout_len) { rc = -ENOSPC; break; }
        memcpy(&out[written], line, n);
        written += n;
    }
    fs_closedir(&dir);
    if (rc == 0) *inout_len = written;
    return rc;
}

static int read_file(const char* rel_path, uint32_t offset, uint8_t* out, uint32_t* inout_len)
{
    char full[128]; join_path(full, sizeof(full), rel_path);
    struct fs_file_t f; fs_file_t_init(&f);
    int rc = fs_open(&f, full, FS_O_READ);
    if (rc != 0) return rc;
    if (offset) {
        rc = fs_seek(&f, offset, FS_SEEK_SET);
        if (rc != 0) { fs_close(&f); return rc; }
    }
    int r = fs_read(&f, out, *inout_len);
    if (r < 0) { rc = r; }
    else { *inout_len = (uint32_t)r; rc = 0; }
    fs_close(&f);
    return rc;
}

static int write_file(const char* rel_path, uint32_t offset, const uint8_t* data, uint32_t len)
{
    char full[128]; join_path(full, sizeof(full), rel_path);
    struct fs_file_t f; fs_file_t_init(&f);
    int rc = fs_open(&f, full, FS_O_CREATE | FS_O_WRITE);
    if (rc != 0) return rc;
    if (offset) {
        rc = fs_seek(&f, offset, FS_SEEK_SET);
        if (rc != 0) { fs_close(&f); return rc; }
    }
    int w = fs_write(&f, data, len);
    if (w < 0 || (uint32_t)w != len) {
        rc = (w < 0) ? w : -EIO;
    } else {
        rc = 0;
    }
    fs_close(&f);
    return rc;
}

static int do_stat(const char* rel_path, uint32_t* out_size, int* out_is_dir)
{
    char full[128]; join_path(full, sizeof(full), rel_path);
    struct fs_dirent ent;
    int rc = fs_stat(full, &ent);
    if (rc == 0) {
        if (out_size) *out_size = (uint32_t)ent.size;
        if (out_is_dir) *out_is_dir = (ent.type == FS_DIR_ENTRY_DIR) ? 1 : 0;
    }
    return rc;
}

static int do_checksum(const char* rel_path, uint32_t* out_crc)
{
    extern uint32_t crc32_update(uint32_t crc, const void* data, size_t len);
    extern uint32_t crc32_finalize(uint32_t crc);
    char full[128]; join_path(full, sizeof(full), rel_path);
    struct fs_file_t f; fs_file_t_init(&f);
    int rc = fs_open(&f, full, FS_O_READ);
    if (rc != 0) return rc;
    uint32_t crc = 0xFFFFFFFFu;
    uint8_t buf[256];
    while (1) {
        int r = fs_read(&f, buf, sizeof(buf));
        if (r < 0) { rc = r; break; }
        if (r == 0) { rc = 0; break; }
        crc = crc32_update(crc, buf, (size_t)r);
    }
    fs_close(&f);
    if (rc == 0 && out_crc) *out_crc = crc32_finalize(crc);
    return rc;
}

static void worker(void* _a, void* _b, void* _c)
{
    ARG_UNUSED(_a); ARG_UNUSED(_b); ARG_UNUSED(_c);
    /* Try mounting at startup; ignore errors until first op */
    (void)ensure_mounted();
    for (;;) {
        sd_req_t* req = NULL;
        k_msgq_get(&sdq, &req, K_FOREVER);
        if (!req) continue;
        int rc = ensure_mounted();
        if (rc != 0 && req->type != REQ_FORMAT) {
            req->rc = rc; k_sem_give(&req->done); continue;
        }
        switch (req->type) {
        case REQ_FORMAT:
            fs_unmount(&s_mnt);
            rc = fs_mkfs(FS_FATFS, (uintptr_t)s_mnt.storage_dev, NULL, 0);
            if (rc == 0) rc = fs_mount(&s_mnt);
            req->rc = rc;
            {
                char dbg[48]; int n = snprintk(dbg, sizeof(dbg), "[sd_fmt] rc=%d", rc);
                if (n > 0) { uint8_t f[128]; size_t fl=0; proto_msg_t m={.cmd=CMD_ID_LOG,.flags=0x02,.payload=(const uint8_t*)dbg,.length=(uint32_t)n}; if (proto_encode(&m,f,sizeof(f),&fl)==PROTO_OK) (void)platform_serial_write(f,fl);}    
            }
            break;
        case REQ_LIST:
            req->rc = list_dir(req->path_a, req->buf, &req->len);
            break;
        case REQ_READ:
            req->rc = read_file(req->path_a, req->offset, req->buf, &req->len);
            break;
        case REQ_WRITE:
            req->rc = write_file(req->path_a, req->offset, req->in, req->len);
            {
                char dbg[48]; int n = snprintk(dbg, sizeof(dbg), "[sd_wr] rc=%d", req->rc);
                if (n > 0) { uint8_t f[128]; size_t fl=0; proto_msg_t m={.cmd=CMD_ID_LOG,.flags=0x02,.payload=(const uint8_t*)dbg,.length=(uint32_t)n}; if (proto_encode(&m,f,sizeof(f),&fl)==PROTO_OK) (void)platform_serial_write(f,fl);}    
            }
            break;
        case REQ_RENAME:
        {
            char fa[128]; char fb[128];
            join_path(fa, sizeof(fa), req->path_a);
            join_path(fb, sizeof(fb), req->path_b);
            req->rc = fs_rename(fa, fb);
            break;
        }
        case REQ_DELETE:
        {
            char fa[128]; join_path(fa, sizeof(fa), req->path_a);
            req->rc = fs_unlink(fa);
            break;
        }
        case REQ_MKDIR:
        {
            char fa[128]; join_path(fa, sizeof(fa), req->path_a);
            req->rc = fs_mkdir(fa);
            break;
        }
        case REQ_STAT:
            req->rc = do_stat(req->path_a, &req->u32_a, &req->i_a);
            break;
        case REQ_CHECKSUM:
            req->rc = do_checksum(req->path_a, &req->u32_a);
            break;
        }
        k_sem_give(&req->done);
    }
}

K_THREAD_STACK_DEFINE(sd_stack, 2048);
static struct k_thread sd_thread;

static int submit(sd_req_t* r)
{
    k_sem_init(&r->done, 0, 1);
    k_msgq_put(&sdq, &r, K_FOREVER);
    k_sem_take(&r->done, K_FOREVER);
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

int sd_list(const char* rel_path, uint8_t* out, uint32_t* inout_len)
{
    sd_req_t r = {.type = REQ_LIST, .path_a = rel_path, .buf = out, .len = inout_len ? *inout_len : 0};
    int rc = submit(&r);
    if (rc == 0 && inout_len) *inout_len = r.len;
    return rc;
}

int sd_read(const char* rel_path, uint32_t offset, uint8_t* out, uint32_t* inout_len)
{
    sd_req_t r = {.type = REQ_READ, .path_a = rel_path, .buf = out, .offset = offset, .len = inout_len ? *inout_len : 0};
    int rc = submit(&r);
    if (rc == 0 && inout_len) *inout_len = r.len;
    return rc;
}

int sd_write(const char* rel_path, uint32_t offset, const uint8_t* data, uint32_t len)
{
    sd_req_t r = {.type = REQ_WRITE, .path_a = rel_path, .in = data, .offset = offset, .len = len};
    return submit(&r);
}

int sd_rename(const char* old_rel_path, const char* new_rel_path)
{
    sd_req_t r = {.type = REQ_RENAME, .path_a = old_rel_path, .path_b = new_rel_path};
    return submit(&r);
}

int sd_delete(const char* rel_path)
{
    sd_req_t r = {.type = REQ_DELETE, .path_a = rel_path};
    return submit(&r);
}

int sd_mkdir(const char* rel_path)
{
    sd_req_t r = {.type = REQ_MKDIR, .path_a = rel_path};
    return submit(&r);
}

int sd_stat_size(const char* rel_path, uint32_t* out_size, int* out_is_dir)
{
    sd_req_t r = {.type = REQ_STAT, .path_a = rel_path};
    int rc = submit(&r);
    if (rc == 0) { if (out_size) *out_size = r.u32_a; if (out_is_dir) *out_is_dir = r.i_a; }
    return rc;
}

int sd_checksum_crc32(const char* rel_path, uint32_t* out_crc)
{
    sd_req_t r = {.type = REQ_CHECKSUM, .path_a = rel_path};
    int rc = submit(&r);
    if (rc == 0 && out_crc) *out_crc = r.u32_a;
    return rc;
}

int sd_status(void)
{
    return ensure_mounted();
}
