#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "app/error_indicator.h"

#if !DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
#warning "Board does not define led0 alias; error indicator disabled"
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#endif

static struct k_thread err_thread;
K_THREAD_STACK_DEFINE(err_stack, 512);
static struct k_mutex err_lock;
static uint32_t err_mask;

static bool led_ready;

static void blink_once(int on_ms, int off_ms)
{
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
    if (led_ready)
    {
        gpio_pin_set_dt(&led0, 1);
        k_sleep(K_MSEC(on_ms));
        gpio_pin_set_dt(&led0, 0);
        k_sleep(K_MSEC(off_ms));
    }
    else
    {
        k_sleep(K_MSEC(on_ms + off_ms));
    }
#else
    k_sleep(K_MSEC(on_ms + off_ms));
#endif
}

static void err_task(void* a, void* b, void* c) // NOLINT(bugprone-easily-swappable-parameters)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    for (;;)
    {
        k_mutex_lock(&err_lock, K_FOREVER);
        uint32_t mask = err_mask;
        k_mutex_unlock(&err_lock);
        if (mask == 0)
        {
            k_sleep(K_MSEC(100));
            continue;
        }

        /* Iterate codes from LSB to MSB; code number is bit index+1. */
        for (unsigned bit = 0; bit < 16; ++bit)
        {
            if ((mask & (1u << bit)) == 0)
            {
                continue;
            }
            unsigned count = bit + 1;
            /* Inter-code separator: longer off gap */
            k_sleep(K_MSEC(200));
            for (unsigned i = 0; i < count; ++i)
            {
                blink_once(150, 150);
            }
            k_sleep(K_MSEC(600));
        }
        /* Big gap between sequences */
        k_sleep(K_MSEC(1200));
    }
}

int error_indicator_init(void)
{
    k_mutex_init(&err_lock);
    err_mask = 0;
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
    led_ready = device_is_ready(led0.port);
    if (led_ready)
    {
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        gpio_flags_t fl =
            (gpio_flags_t)((unsigned long)GPIO_OUTPUT | (unsigned long)GPIO_OUTPUT_INIT_LOW);
        (void)gpio_pin_configure_dt(&led0, fl);
    }
#else
    led_ready = false;
#endif
    k_thread_create(&err_thread, err_stack, K_THREAD_STACK_SIZEOF(err_stack), err_task, NULL, NULL,
                    NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(&err_thread, "err_led");
    return 0;
}

void error_indicator_set(uint32_t mask)
{
    k_mutex_lock(&err_lock, K_FOREVER);
    err_mask = mask;
    k_mutex_unlock(&err_lock);
}

void error_indicator_add(uint32_t bits)
{
    k_mutex_lock(&err_lock, K_FOREVER);
    err_mask |= bits;
    k_mutex_unlock(&err_lock);
}

void error_indicator_clear(uint32_t bits)
{
    k_mutex_lock(&err_lock, K_FOREVER);
    err_mask &= ~bits;
    k_mutex_unlock(&err_lock);
}

bool error_indicator_active(void)
{
    k_mutex_lock(&err_lock, K_FOREVER);
    bool active = (err_mask != 0);
    k_mutex_unlock(&err_lock);
    return active;
}
