#include "cmd/dispatch.h"

/* Optional debug hook from app (weak). Always callable; may be a no-op. */
extern void proto_debug_log(const char* msg);

#define MAX_CMDS 32

typedef struct
{
    uint16_t id;
    cmd_handler_fn fn;
} cmd_entry_t;

static cmd_entry_t g_cmds[MAX_CMDS];
static uint32_t g_cmd_count;

int cmd_register(uint16_t cmd_id, cmd_handler_fn fn)
{
    int rc = 0;
    if (g_cmd_count >= MAX_CMDS)
    {
        rc = -1;
    }
    else
    {
        g_cmds[g_cmd_count++] = (cmd_entry_t){.id = cmd_id, .fn = fn};
        {
        char dbg[64];
        unsigned n = (unsigned)g_cmd_count;
        /* Report: [cmd_register] id=0xXXXX count=N */
        static const char hex[] = "0123456789abcdef";
        int p = 0;
        const char* pre = "[cmd_register] id=0x";
        while (*pre)
        {
            dbg[p++] = *pre++;
        }
        dbg[p++] = hex[(cmd_id >> 12) & 0xF];
        dbg[p++] = hex[(cmd_id >> 8) & 0xF];
        dbg[p++] = hex[(cmd_id >> 4) & 0xF];
        dbg[p++] = hex[cmd_id & 0xF];
        const char* mid = " count=";
        pre = mid;
        while (*pre)
        {
            dbg[p++] = *pre++;
        }
        char tmp[10];
        int ti = 0;
        if (n == 0)
        {
            tmp[ti++] = '0';
        }
        else
        {
            unsigned t = n;
            char rev[10];
            int ri = 0;
            while (t && ri < 10)
            {
                rev[ri++] = (char)('0' + (t % 10));
                t /= 10;
            }
            while (ri)
            {
                tmp[ti++] = rev[--ri];
            }
        }
        for (int i = 0; i < ti; i++)
        {
            dbg[p++] = tmp[i];
        }
        dbg[p] = 0;
        proto_debug_log(dbg);
        }
    }
    return rc;
}

int cmd_dispatch(uint16_t cmd_id, const uint8_t* req, uint32_t req_len, uint8_t* rsp,
                 uint32_t* rsp_len)
{
    int rc = -1;
    for (uint32_t i = 0; i < g_cmd_count; ++i)
    {
        if (g_cmds[i].id == cmd_id)
        {
            rc = g_cmds[i].fn ? g_cmds[i].fn(req, req_len, rsp, rsp_len) : -1;
            goto out;
        }
    }
    {
        char dbg[64];
        int p = 0;
        const char* pre = "[cmd_dispatch_miss] id=0x";
        while (*pre)
        {
            dbg[p++] = *pre++;
        }
        static const char hex[] = "0123456789abcdef";
        dbg[p++] = hex[(cmd_id >> 12) & 0xF];
        dbg[p++] = hex[(cmd_id >> 8) & 0xF];
        dbg[p++] = hex[(cmd_id >> 4) & 0xF];
        dbg[p++] = hex[cmd_id & 0xF];
        const char* suf = " count=";
        pre = suf;
        while (*pre)
            dbg[p++] = *pre++;
        unsigned n = (unsigned)g_cmd_count;
        char tmp[10];
        int ti = 0;
        if (n == 0)
        {
            tmp[ti++] = '0';
        }
        else
        {
            unsigned t = n;
            char rev[10];
            int ri = 0;
            while (t && ri < 10)
            {
                rev[ri++] = (char)('0' + (t % 10));
                t /= 10;
            }
            while (ri)
            {
                tmp[ti++] = rev[--ri];
            }
        }
        for (int i = 0; i < ti; i++)
        {
            dbg[p++] = tmp[i];
        }
        dbg[p] = 0;
        proto_debug_log(dbg);
    }
out:
    return rc;
}
