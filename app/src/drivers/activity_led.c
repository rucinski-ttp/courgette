#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "app/activity_led.h"

#if !DT_NODE_HAS_STATUS(DT_ALIAS(led1), okay)
#warning "Board does not define led1 alias; activity LED disabled"
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(led1), okay)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static struct k_timer off_timer;

static void off_timer_handler(struct k_timer* timer)
{
    ARG_UNUSED(timer);
    gpio_pin_set_dt(&led, 0);
}
#endif

int activity_led_init(void)
{
#if DT_NODE_HAS_STATUS(DT_ALIAS(led1), okay)
    if (!device_is_ready(led.port))
    {
        return -ENODEV;
    }
    gpio_flags_t fl = (gpio_flags_t)((unsigned long)GPIO_OUTPUT | (unsigned long)GPIO_OUTPUT_INIT_LOW); // NOLINT(hicpp-signed-bitwise)
    int ret = gpio_pin_configure_dt(&led, fl);
    if (ret)
    {
        return ret;
    }
    k_timer_init(&off_timer, off_timer_handler, NULL);
    return 0;
#else
    return 0;
#endif
}

void activity_led_pulse(void)
{
#if DT_NODE_HAS_STATUS(DT_ALIAS(led1), okay)
    gpio_pin_set_dt(&led, 1);
    k_timer_start(&off_timer, K_MSEC(60), K_NO_WAIT);
#endif
}
