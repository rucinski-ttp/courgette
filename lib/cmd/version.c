#include "cmd/dispatch.h"
#include <string.h>

#ifndef APP_GIT_HASH
#define APP_GIT_HASH "unknown"
#endif

static int handle_version(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req;
    (void)req_len;
    const char* s = APP_GIT_HASH;
    uint32_t n = (uint32_t)strlen(s);
    if (rsp && rsp_len)
    {
        if (*rsp_len < n)
        {
            return -1;
        }
        for (uint32_t i = 0; i < n; ++i)
        {
            rsp[i] = (uint8_t)s[i];
        }
        *rsp_len = n;
    }
    return 0;
}

int cmd_version_init(void) { return cmd_register(CMD_ID_VERSION, handle_version); }
