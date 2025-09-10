#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

/* Minimal, robust fatal error indicator that does not depend on
 * application subsystems having initialized. Uses led0 alias. */

#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#endif

/* Blink N short pulses on led0 with busy-wait delays (safe in fatal). */
static void fatal_blink(unsigned count)
{
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
    if (!device_is_ready(led0.port))
    {
        return;
    }
    gpio_flags_t fl = (gpio_flags_t)((unsigned long)GPIO_OUTPUT | (unsigned long)GPIO_OUTPUT_INIT_LOW); // NOLINT(hicpp-signed-bitwise)
    (void)gpio_pin_configure_dt(&led0, fl);
    for (unsigned i = 0; i < count; ++i)
    {
        gpio_pin_set_dt(&led0, 1);
        k_busy_wait(120 * 1000);
        gpio_pin_set_dt(&led0, 0);
        k_busy_wait(120 * 1000);
    }
#else
    ARG_UNUSED(count);
#endif
}

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t* esf)
{
    ARG_UNUSED(esf);
    /* 4-blink repeating pattern to indicate fatal error. */
    for (;;)
    {
        fatal_blink(4);
        k_busy_wait(800 * 1000);
    }
}
