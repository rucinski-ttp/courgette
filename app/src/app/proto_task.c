#include <zephyr/kernel.h>

#include "app/activity_led.h"
#include "app/serial_async.h"
#include "cmd/dispatch.h"
#include "protocol/protocol.h"

static proto_stream_t s_stream;

static void on_msg(const proto_msg_t* msg, void* user)
{
    (void)user;
    uint8_t rsp[256];
    uint32_t rsp_len = sizeof(rsp);
    int rc = cmd_dispatch(msg->cmd, msg->payload, msg->length, rsp, &rsp_len);
    if (msg->cmd == CMD_ID_MEM_READ || msg->cmd == CMD_ID_MEM_WRITE)
    {
        activity_led_pulse();
    }

    uint8_t frame[320];
    proto_msg_t out = {0};
    out.cmd = msg->cmd;
    out.flags = 0x01 /* response */ | (rsp_len ? 0x02 : 0x00);
    out.payload = (rsp_len ? rsp : NULL);
    out.length = (rsp_len ? rsp_len : 0);
    size_t frame_len = 0;
    if (rc == 0 && proto_encode(&out, frame, sizeof(frame), &frame_len) == PROTO_OK)
    {
        (void)platform_serial_write(frame, frame_len);
    }
}

static void proto_thread(void* a, void* b, void* c)
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
    k_thread_create(&proto_thread_desc, proto_stack, K_THREAD_STACK_SIZEOF(proto_stack),
                    proto_thread, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(&proto_thread_desc, "proto");
}
