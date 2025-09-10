/* UART RX/TX ISR with ring buffers (non-blocking TX). */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include "app/serial_async.h"
#include "util/ring_buffer.h"

#define RX_BUF_SZ 2048
#define TX_BUF_SZ 8192

static const struct device* uart_dev;
static rb_t rx_rb;
static uint8_t rx_storage[RX_BUF_SZ];
static rb_t tx_rb;
static uint8_t tx_storage[TX_BUF_SZ];
static struct k_mutex tx_mutex;

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

        if (uart_irq_tx_ready(dev))
        {
            /* Feed UART TX FIFO from tx_rb until empty or FIFO full */
            while (uart_irq_tx_ready(dev))
            {
                const uint8_t* ptr = NULL;
                unsigned int key = irq_lock();
                size_t avail = rb_peek_contig(&tx_rb, &ptr);
                if (avail == 0)
                {
                    uart_irq_tx_disable(dev);
                    irq_unlock(key);
                    break;
                }
                /* Limit per-fill to keep ISR responsive */
                if (avail > 64)
                {
                    avail = 64;
                }
                int wrote = uart_fifo_fill(dev, ptr, (int)avail);
                if (wrote > 0)
                {
                    rb_consume(&tx_rb, (size_t)wrote);
                }
                irq_unlock(key);
                if (wrote <= 0)
                {
                    /* FIFO not ready; try later */
                    break;
                }
            }
        }
    }
}

int platform_serial_init(void)
{
    /* Bind strictly to zephyr,console as declared by board DTS. */
    uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(uart_dev))
    {
        return -ENODEV;
    }
    rb_init(&rx_rb, rx_storage, sizeof(rx_storage));
    rb_init(&tx_rb, tx_storage, sizeof(tx_storage));
    k_mutex_init(&tx_mutex);
#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
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

int platform_serial_write(const uint8_t* data, size_t len)
{
    if (!len)
    {
        return 0;
    }
    if (!uart_dev || !device_is_ready(uart_dev))
    {
        return -ENODEV;
    }
    k_mutex_lock(&tx_mutex, K_FOREVER);

    /* For very large frames, fall back to direct poll-out to avoid
     * any chance of TX ISR underflow corrupting payload. */
    if (len >= ((size_t)TX_BUF_SZ * 2u))
    {
        /* Wait for any pending ISR-driven TX to drain */
        for (int spins = 0; spins < 200; ++spins)
        {
            if (rb_used(&tx_rb) == 0)
            {
                break;
            }
            k_sleep(K_MSEC(1));
        }
        uart_irq_tx_disable(uart_dev);
        for (size_t i = 0; i < len; ++i)
        {
            uart_poll_out(uart_dev, data[i]);
        }
        k_mutex_unlock(&tx_mutex);
        return 0;
    }

    size_t i = 0;
    while (i < len)
    {
        /* Copy in small chunks under IRQ lock to avoid long interrupt latency */
        size_t remaining = len - i;
        size_t chunk = remaining > 64 ? 64 : remaining;
        unsigned int key = irq_lock();
        size_t enq = rb_write(&tx_rb, &data[i], chunk);
        irq_unlock(key);
        i += enq;

        /* Prime TX FIFO with a few bytes to ensure an interrupt will follow */
        (void)uart_irq_update(uart_dev);
        unsigned int key2 = irq_lock();
        const uint8_t* p = NULL;
        size_t avail = rb_peek_contig(&tx_rb, &p);
        if (avail > 0)
        {
            if (avail > 64)
            {
                avail = 64;
            }
            (void)uart_irq_update(uart_dev);
            if (uart_irq_tx_ready(uart_dev))
            {
                int sent = uart_fifo_fill(uart_dev, p, (int)avail);
                if (sent > 0)
                {
                    rb_consume(&tx_rb, (size_t)sent);
                }
            }
        }
        irq_unlock(key2);
        /* Enable TX IRQ to continue sending remaining data */
        uart_irq_tx_enable(uart_dev);

        if (i < len && enq == 0)
        {
            /* Wait briefly to free space; yield CPU to ISR */
            k_sleep(K_MSEC(1));
        }
    }
    k_mutex_unlock(&tx_mutex);
    return 0;
}

size_t platform_serial_read(uint8_t* out, size_t max_len)
{
    if (!uart_dev || !device_is_ready(uart_dev))
    {
        return 0;
    }
    return rb_read(&rx_rb, out, max_len);
}
