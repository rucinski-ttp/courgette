#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "app/heartbeat.h"
#include "app/serial.h"

void main(void)
{
    printk("\n[BOOT] Zephyr STM32H747I-DISCO bring-up (M7)\n");

    /* Platform init (stub for now) */
    (void)platform_serial_init();

    /* Heartbeat LED */
    if (heartbeat_init() != 0)
    {
        printk("[heartbeat] init failed\n");
    }

    uint32_t tick = 0;
    while (1)
    {
        k_sleep(K_MSEC(1000));
        tick++;
        printk("[tick] %lu\n", (unsigned long)tick);
    }
}
