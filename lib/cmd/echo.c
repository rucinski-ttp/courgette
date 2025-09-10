#include "cmd/dispatch.h"
#include <string.h>

static int handle_echo(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    if (rsp && rsp_len)
    {
        if (*rsp_len < req_len)
        {
            return -1;
        }
        for (uint32_t i = 0; i < req_len; ++i)
        {
            rsp[i] = req[i];
        }
        *rsp_len = req_len;
    }
    return 0;
}

int cmd_echo_init(void) { return cmd_register(CMD_ID_ECHO, handle_echo); }
