#include <string.h>

#include "app/doom_task.h"
#include "app/serial_async.h"
#include "cmd/dispatch.h"
#include "protocol/protocol.h"

static void wr_le32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8u) & 0xFFu);
    p[2] = (uint8_t)((v >> 16u) & 0xFFu);
    p[3] = (uint8_t)((v >> 24u) & 0xFFu);
}

static int h_start(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req;
    (void)req_len;
    (void)rsp;
    if (rsp_len)
    {
        *rsp_len = 0;
    }
    int rc = doom_task_start();
    /* Emit a protocol log so host can see we started */
    const char* msg = "[DOOM] start";
    uint8_t frame[64];
    size_t flen = 0;
    proto_msg_t m = {.cmd = CMD_ID_LOG,
                     .flags = 0x02,
                     .payload = (const uint8_t*)msg,
                     .length = (uint32_t)strlen(msg)};
    if (proto_encode(&m, frame, sizeof(frame), &flen) == PROTO_OK)
    {
        (void)platform_serial_write(frame, flen);
    }
    return rc;
}

static int h_status(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req;
    (void)req_len;
    if (!rsp || !rsp_len || *rsp_len < 8)
    {
        return -1;
    }
    doom_state_t st;
    uint32_t ticks;
    doom_task_status(&st, &ticks);
    wr_le32(&rsp[0], (uint32_t)st);
    wr_le32(&rsp[4], ticks);
    *rsp_len = 8;
    return 0;
}

static int h_stop(const uint8_t* req, uint32_t req_len, uint8_t* rsp, uint32_t* rsp_len)
{
    (void)req;
    (void)req_len;
    (void)rsp;
    if (rsp_len)
    {
        *rsp_len = 0;
    }
    return doom_task_stop();
}

int cmd_doom_init(void)
{
    (void)cmd_register(CMD_ID_DOOM_START, h_start);
    (void)cmd_register(CMD_ID_DOOM_STATUS, h_status);
    (void)cmd_register(CMD_ID_DOOM_STOP, h_stop);
    return 0;
}
