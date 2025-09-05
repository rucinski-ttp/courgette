/* UART RX polling thread + TX poll-out; non-blocking to app via internal buffers/threads. */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include "app/serial_async.h"
#include "util/ring_buffer.h"

#define RX_BUF_SZ 512
#define TX_BUF_SZ 512

static const struct device* uart_dev;
static rb_t rx_rb;
static uint8_t rx_storage[RX_BUF_SZ];

static void uart_isr(const struct device* dev, void* user)
{
    ARG_UNUSED(user);
    while (uart_irq_update(dev) && uart_irq_is_pending(dev))
    {
        if (uart_irq_rx_ready(dev))
        {
            uint8_t buf[64];
            int got = uart_fifo_read(dev, buf, sizeof(buf));
            if (got > 0)
            {
                (void)rb_write(&rx_rb, buf, (size_t)got);
            }
        }
    }
}

int platform_serial_init(void)
{
    uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(uart_dev))
    {
        return -ENODEV;
    }
    rb_init(&rx_rb, rx_storage, sizeof(rx_storage));
    /* Install ISR and enable RX */
#if defined(CONFIG_UART_INTERRUPT_DRIVEN) || 1
/* Prefer user_data API when available; Zephyr keeps both for compatibility */
#ifdef uart_irq_callback_user_data_set
    uart_irq_callback_user_data_set(uart_dev, uart_isr, NULL);
#else
    uart_irq_callback_set(uart_dev, uart_isr);
#endif
    uart_irq_rx_enable(uart_dev);
#endif
    return 0;
}

size_t platform_serial_available(void) { return rb_used(&rx_rb); }

size_t platform_serial_read(uint8_t* out, size_t max_len) { return rb_read(&rx_rb, out, max_len); }

int platform_serial_write(const uint8_t* data, size_t len)
{
    if (!len)
        return 0;
    /* Send immediately using polling; returns quickly for small frames. */
    for (size_t i = 0; i < len; ++i)
    {
        uart_poll_out(uart_dev, data[i]);
    }
    return 0;
}
