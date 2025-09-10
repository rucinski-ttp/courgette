#include <zephyr/kernel.h>

#include "app/activity_led.h"
#include "app/sd_ops.h"
#include "app/serial_async.h"
#include "cmd/dispatch.h"
#include "protocol/protocol.h"
#include <string.h>

static proto_stream_t s_stream;

/* Silence protocol-layer debug on device to avoid TX during RX. */
void proto_debug_log(const char* msg) { (void)msg; }

/* Large shared buffers to support big SD reads in one frame */
static uint8_t g_rsp_buf[65536];
static uint8_t g_frame_buf[65536 + 32];

static void on_msg(const proto_msg_t* msg, void* user)
{
    (void)user;
    uint8_t* rsp = g_rsp_buf;
    uint32_t rsp_len = (uint32_t)sizeof(g_rsp_buf);
    int rc = cmd_dispatch(msg->cmd, msg->payload, msg->length, rsp, &rsp_len);
    /* keep quiet in production to avoid interleaving on the link */
    if (msg->cmd == CMD_ID_MEM_READ || msg->cmd == CMD_ID_MEM_WRITE)
    {
        activity_led_pulse();
    }

    /* Always return the handler's payload, even on error. Handlers encode rc
     * into payload when appropriate (e.g., SD.Status). */
    uint8_t* frame = g_frame_buf;
    proto_msg_t out = {0};
    out.cmd = msg->cmd;
    uint32_t eff_len = (rc == 0) ? rsp_len : 0u;
    out.flags = (uint8_t)(0x01u /* response */ | (eff_len ? 0x02u : 0x00u));
    out.payload = (eff_len ? rsp : NULL);
    out.length = eff_len;
    size_t frame_len = 0;
    if (proto_encode(&out, frame, (size_t)sizeof(g_frame_buf), &frame_len) == PROTO_OK)
    {
        (void)platform_serial_write(frame, frame_len);
    }
}

static void proto_thread(void* a, void* b, void* c) // NOLINT(bugprone-easily-swappable-parameters)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);
    proto_stream_init(&s_stream);
    uint8_t tmp[128];
    for (;;)
    {
        size_t n = platform_serial_read(tmp, sizeof(tmp));
        if (n > 0)
        {
            proto_stream_feed(&s_stream, tmp, n, on_msg, NULL);
        }
        else
        {
            k_sleep(K_MSEC(5));
        }
    }
}

K_THREAD_STACK_DEFINE(proto_stack, 2048);
static struct k_thread proto_thread_desc;

void proto_task_start(void)
{
    /* No verbose self-test or RX debug spam in production. */
    k_thread_create(&proto_thread_desc, proto_stack, K_THREAD_STACK_SIZEOF(proto_stack),
                    proto_thread, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(&proto_thread_desc, "proto");
}

/* Emit a periodic LOG frame so host tests can observe liveness. */
static void tick_thread(void* a, void* b, void* c) // NOLINT(bugprone-easily-swappable-parameters)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);
    for (;;)
    {
        const char* s = "[tick]";
        uint8_t f[32];
        size_t fl = 0;
        proto_msg_t m = {.cmd = CMD_ID_LOG,
                         .flags = 0x02,
                         .payload = (const uint8_t*)s,
                         .length = (uint32_t)strlen(s)};
        if (proto_encode(&m, f, sizeof(f), &fl) == PROTO_OK)
        {
            (void)platform_serial_write(f, fl);
        }
        k_sleep(K_MSEC(1000));
    }
}

K_THREAD_STACK_DEFINE(tick_stack, 512);
static struct k_thread tick_thread_desc;

void proto_start_tick(void)
{
    k_thread_create(&tick_thread_desc, tick_stack, K_THREAD_STACK_SIZEOF(tick_stack), tick_thread,
                    NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(&tick_thread_desc, "tick");
}
