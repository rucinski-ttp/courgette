#include <string.h>
#include <zephyr/fatal.h>
#include <zephyr/kernel.h>

#include "app/serial_async.h"
#include "protocol/protocol.h"

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t* esf)
{
    ARG_UNUSED(esf);
    const char* base = "[fatal]";
    uint8_t frame[128];
    size_t fl = 0;
    char msg[64];
    int n = snprintk(msg, sizeof(msg), "%s reason=%u", base, reason);
    if (n > 0)
    {
        proto_msg_t m = {.cmd = 0x00FE /* CMD_LOG */,
                         .flags = 0x02,
                         .payload = (const uint8_t*)msg,
                         .length = (uint32_t)n};
        if (proto_encode(&m, frame, sizeof(frame), &fl) == PROTO_OK)
        {
            (void)platform_serial_write(frame, fl);
        }
    }
    k_fatal_halt(reason);
}
