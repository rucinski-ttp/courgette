#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "app/activity_led.h"
#include "app/heartbeat.h"
#include "app/serial_async.h"
#include "cmd/dispatch.h"
#include "cmd/echo.h"
#include "cmd/mem.h"
#include "cmd/reboot.h"
#include "cmd/version.h"
#include "protocol/protocol.h"
#include <string.h>
#include <zephyr/kernel.h>

/* start protocol thread */
void proto_task_start(void);

void main(void)
{
    /* Platform serial (async) */
    if (platform_serial_init() != 0)
    {
        /* If UART fails, nothing else will work */
    }

    /* Register commands */
    (void)cmd_echo_init();
    (void)cmd_version_init();
    (void)cmd_reboot_init();
    (void)cmd_mem_init();

    /* Start protocol processing after handlers are registered */
    proto_task_start();

    /* Heartbeat LED */
    if (heartbeat_init() != 0)
    {
        /* ignore */
    }

    /* Activity LED (blink on mem read/write) */
    (void)activity_led_init();

    /* Send boot log over protocol */
    {
        const char* boot = "[BOOT] Zephyr STM32H747I-DISCO bring-up (M7)";
        uint8_t frame[256];
        size_t flen = 0;
        proto_msg_t m = {.cmd = CMD_ID_LOG,
                         .flags = 0x02,
                         .payload = (const uint8_t*)boot,
                         .length = (uint32_t)strlen(boot)};
        if (proto_encode(&m, frame, sizeof(frame), &flen) == PROTO_OK)
        {
            (void)platform_serial_write(frame, flen);
        }
    }

    uint32_t tick = 0;
    while (1)
    {
        k_sleep(K_MSEC(1000));
        tick++;
        char msg[32];
        int n = snprintk(msg, sizeof(msg), "[tick] %lu", (unsigned long)tick);
        if (n > 0)
        {
            uint8_t frame[96];
            size_t flen = 0;
            proto_msg_t m = {.cmd = CMD_ID_LOG,
                             .flags = 0x02,
                             .payload = (const uint8_t*)msg,
                             .length = (uint32_t)n};
            if (proto_encode(&m, frame, sizeof(frame), &flen) == PROTO_OK)
            {
                (void)platform_serial_write(frame, flen);
            }
        }
    }
}
