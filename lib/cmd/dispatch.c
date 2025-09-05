#include "cmd/dispatch.h"

#define MAX_CMDS 16

typedef struct
{
    uint16_t id;
    cmd_handler_fn fn;
} cmd_entry_t;

static cmd_entry_t g_cmds[MAX_CMDS];
static uint32_t g_cmd_count;

int cmd_register(uint16_t cmd_id, cmd_handler_fn fn)
{
    if (g_cmd_count >= MAX_CMDS)
        return -1;
    g_cmds[g_cmd_count++] = (cmd_entry_t){.id = cmd_id, .fn = fn};
    return 0;
}

int cmd_dispatch(uint16_t cmd_id, const uint8_t* req, uint32_t req_len, uint8_t* rsp,
                 uint32_t* rsp_len)
{
    for (uint32_t i = 0; i < g_cmd_count; ++i)
    {
        if (g_cmds[i].id == cmd_id)
        {
            return g_cmds[i].fn ? g_cmds[i].fn(req, req_len, rsp, rsp_len) : -1;
        }
    }
    return -1;
}
