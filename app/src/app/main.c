#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "app/activity_led.h"
#include "app/app_runtime.h"
#include "app/cmd_display.h"
#include "app/cmd_doom.h"
#include "app/cmd_sd.h"
#include "app/error_indicator.h"
#include "app/heartbeat.h"
#include "app/proto_task.h"
#include "app/sd_ops.h"
#include "app/serial_async.h"
#include "cmd/dispatch.h"
#include "cmd/echo.h"
#include "cmd/mem.h"
#include "cmd/reboot.h"
#include "cmd/version.h"
#include "protocol/protocol.h"
#include <string.h>
#include <zephyr/kernel.h>

/* SD warmup thread to nudge early mount */
static void sd_warmup(void* a, void* b, void* c) // NOLINT(bugprone-easily-swappable-parameters)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);
    k_sleep(K_MSEC(150));
    (void)sd_status();
}
K_THREAD_STACK_DEFINE(sd_warm_stack, 512);
static struct k_thread sd_warm_desc;

void main(void)
{
    /* Heartbeat LED early so we can observe liveness regardless of UART */
    (void)heartbeat_init();
    (void)error_indicator_init();

    /* Platform serial (async). If it fails, proceed so heartbeat still runs
     * for diagnosis (no fallbacks will be used in handlers). */
    if (platform_serial_init() != 0)
    {
        /* Flag UART not ready: code 1 */
        error_indicator_add(ERR_UART_NOT_READY);
    }

    /* Register commands */
    (void)cmd_echo_init();
    (void)cmd_version_init();
    (void)cmd_reboot_init();
    (void)cmd_mem_init();
    (void)cmd_sd_init();
    (void)cmd_display_init();
    (void)cmd_doom_init();

    /* Start protocol processing after handlers are registered */
    proto_task_start();
    /* Start periodic tick LOGs for integration checks */
    proto_start_tick();

    /* Heartbeat already started above */

    /* Activity LED (blink on mem read/write) */
    (void)activity_led_init();

    /* SD ops worker. Do not touch the SD card at boot; defer mounting until
     * the host sends an SD command. This avoids early-driver races that have
     * been causing boot-time faults on some boards. */
    (void)sd_ops_init();

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

    /* Signal readiness after handlers and workers are up */
    {
        const char* ready = "[READY]";
        uint8_t frame[64];
        size_t flen = 0;
        proto_msg_t m = {.cmd = CMD_ID_LOG,
                         .flags = 0x02,
                         .payload = (const uint8_t*)ready,
                         .length = (uint32_t)strlen(ready)};
        if (proto_encode(&m, frame, sizeof(frame), &flen) == PROTO_OK)
        {
            (void)platform_serial_write(frame, flen);
        }
    }

    /* Run idle loop outside of main.c for clarity */
    app_run_forever();
}
