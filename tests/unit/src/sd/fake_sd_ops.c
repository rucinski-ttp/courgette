#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "app/sd_ops.h"
#include "util/crc32.h"

static char g_root[512] = {0};

static void ensure_root(void)
{
    if (g_root[0] == '\0')
    {
        const char* env = getenv("SD_HOST_ROOT");
        if (!env || !*env)
            env = "sd_host_root";
        snprintf(g_root, sizeof(g_root), "%s", env);
        mkdir(g_root, 0777);
    }
}

int sd_ops_init(void)
{
    ensure_root();
    return 0;
}

static void join(char* out, size_t cap, const char* rel)
{
    ensure_root();
    if (!rel || rel[0] == '\0' || strcmp(rel, "/") == 0)
    {
        snprintf(out, cap, "%s", g_root);
        return;
    }
    const char* p = rel;
    if (*p == '/')
        p++;
    snprintf(out, cap, "%s/%s", g_root, p);
}

int sd_status(void)
{
    ensure_root();
    return 0;
}

int sd_format_sync(void)
{
    ensure_root();
    /* Naive recursive delete of g_root contents */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'/*", g_root);
    (void)system(cmd);
    return 0;
}

int sd_format(void) { return sd_format_sync(); }

int sd_mkdir(const char* rel)
{
    char p[1024];
    join(p, sizeof(p), rel);
    return mkdir(p, 0777) == 0 ? 0 : (errno == EEXIST ? 0 : -1);
}

int sd_delete(const char* rel)
{
    char p[1024];
    join(p, sizeof(p), rel);
    return remove(p) == 0 ? 0 : -1;
}

int sd_rename(const char* old_rel, const char* new_rel)
{
    char a[1024];
    char b[1024];
    join(a, sizeof(a), old_rel);
    join(b, sizeof(b), new_rel);
    return rename(a, b) == 0 ? 0 : -1;
}

int sd_list(const char* rel, uint8_t* out, uint32_t* inout_len)
{
    char p[1024];
    join(p, sizeof(p), rel);
    DIR* d = opendir(p);
    if (!d)
        return -1;
    uint32_t w = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL)
    {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        char fp[1200];
        snprintf(fp, sizeof(fp), "%s/%s", p, e->d_name);
        struct stat st;
        if (stat(fp, &st) != 0)
            continue;
        char line[256];
        if (S_ISDIR(st.st_mode))
            snprintf(line, sizeof(line), "D %s\n", e->d_name);
        else
            snprintf(line, sizeof(line), "F %s %u\n", e->d_name, (unsigned)st.st_size);
        size_t n = strlen(line);
        if (w + n > *inout_len)
        {
            closedir(d);
            return -1;
        }
        memcpy(out + w, line, n);
        w += (uint32_t)n;
    }
    closedir(d);
    *inout_len = w;
    return 0;
}

int sd_read(const char* rel, uint32_t offset, uint8_t* out, uint32_t* inout_len)
{
    char p[1024];
    join(p, sizeof(p), rel);
    FILE* f = fopen(p, "rb");
    if (!f)
        return -1;
    if (fseek(f, (long)offset, SEEK_SET) != 0)
    {
        fclose(f);
        return -1;
    }
    size_t r = fread(out, 1, *inout_len, f);
    fclose(f);
    *inout_len = (uint32_t)r;
    return 0;
}

int sd_write(const char* rel, uint32_t offset, const uint8_t* data, uint32_t len)
{
    char p[1024];
    join(p, sizeof(p), rel);
    /* Ensure parent directories exist */
    char* slash = strrchr(p, '/');
    if (slash)
    {
        *slash = '\0';
        (void)mkdir(p, 0777);
        *slash = '/';
    }
    FILE* f = fopen(p, "r+b");
    if (!f)
        f = fopen(p, "w+b");
    if (!f)
        return -1;
    if (fseek(f, (long)offset, SEEK_SET) != 0)
    {
        fclose(f);
        return -1;
    }
    size_t w = fwrite(data, 1, len, f);
    fflush(f);
    fclose(f);
    return (w == len) ? 0 : -1;
}

int sd_stat_size(const char* rel, uint32_t* out_size, int* out_is_dir)
{
    char p[1024];
    join(p, sizeof(p), rel);
    struct stat st;
    if (stat(p, &st) != 0)
        return -1;
    if (out_size)
        *out_size = (uint32_t)st.st_size;
    if (out_is_dir)
        *out_is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
    return 0;
}

int sd_checksum_crc32(const char* rel, uint32_t* out_crc)
{
    char p[1024];
    join(p, sizeof(p), rel);
    FILE* f = fopen(p, "rb");
    if (!f)
        return -1;
    uint32_t crc = 0xFFFFFFFFu;
    uint8_t buf[256];
    for (;;)
    {
        size_t r = fread(buf, 1, sizeof(buf), f);
        if (r == 0)
            break;
        crc = crc32_update(crc, buf, r);
    }
    fclose(f);
    if (out_crc)
        *out_crc = crc32_finalize(crc);
    return 0;
}

const char* SD_MOUNT_POINT = "/host";

int sd_fill_pattern(const char* rel, uint32_t size, uint32_t seed)
{
    char p[1024];
    join(p, sizeof(p), rel);
    /* Ensure parent directories exist */
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", p);
    char* slash = strrchr(tmp, '/');
    if (slash)
    {
        *slash = '\0';
        (void)mkdir(tmp, 0777);
        *slash = '/';
    }
    FILE* f = fopen(p, "w+b");
    if (!f)
        return -1;
    uint32_t x = seed ? seed : 0x12345678u;
    uint8_t buf[1024];
    uint32_t remaining = size;
    while (remaining > 0)
    {
        uint32_t n = remaining > sizeof(buf) ? sizeof(buf) : remaining;
        for (uint32_t i = 0; i < n; ++i)
        {
            x ^= (x << 13);
            x ^= (x >> 17);
            x ^= (x << 5);
            buf[i] = (uint8_t)x;
        }
        size_t w = fwrite(buf, 1, n, f);
        if (w != n)
        {
            fclose(f);
            return -1;
        }
        remaining -= (uint32_t)w;
    }
    fflush(f);
    fclose(f);
    return 0;
}
