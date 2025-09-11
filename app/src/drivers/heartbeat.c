#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "app/error_indicator.h"
#include "app/heartbeat.h"

#if !DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
#error "Board does not define led0 alias"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static K_THREAD_STACK_DEFINE(heartbeat_stack, 512);
static struct k_thread heartbeat_thread;

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static void heartbeat_entry(void* a, void* b, void* c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    int level = 0;
    for (;;)
    {
        if (error_indicator_active())
        {
            /* Yield to error indicator; keep LED off to avoid pattern clash */
            gpio_pin_set_dt(&led, 0);
            k_sleep(K_MSEC(100));
            continue;
        }
        gpio_pin_set_dt(&led, level);
        level = !level;
        k_sleep(K_MSEC(500));
    }
}

int heartbeat_init(void)
{
    if (!device_is_ready(led.port))
    {
        return -ENODEV;
    }

    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    gpio_flags_t fl =
        (gpio_flags_t)((unsigned long)GPIO_OUTPUT | (unsigned long)GPIO_OUTPUT_INIT_LOW);
    int ret = gpio_pin_configure_dt(&led, fl);
    if (ret)
    {
        return ret;
    }

    k_thread_create(&heartbeat_thread, heartbeat_stack, K_THREAD_STACK_SIZEOF(heartbeat_stack),
                    heartbeat_entry, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0,
                    K_NO_WAIT);
    k_thread_name_set(&heartbeat_thread, "heartbeat");
    return 0;
}
