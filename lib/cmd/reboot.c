#include "cmd/dispatch.h"
#include <zephyr/sys/reboot.h>

static int handle_reboot(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req;
    (void)req_len;
    (void)rsp;
    (void)rsp_len;
    sys_reboot(SYS_REBOOT_COLD);
    return 0;
}

int cmd_reboot_init(void) { return cmd_register(CMD_ID_REBOOT, handle_reboot); }
