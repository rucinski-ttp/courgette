/* Protocol encode/decode and streaming parser */
#include "protocol/protocol.h"
#include "util/crc32.h"

static inline uint16_t le16(uint16_t v) { return v; }
static inline uint32_t le32(uint32_t v) { return v; }

void proto_init(proto_ctx_t* ctx)
{
    if (ctx)
    {
        ctx->version = PROTO_VERSION;
    }
}

proto_rc_t proto_encode(const proto_msg_t* msg, uint8_t* out, size_t out_cap, size_t* out_len)
{
    if (!msg || !out)
        return PROTO_ERR;
    const size_t hdr_len = 4 /*magic*/ + 1 + 1 + 2 + 4 + 4;
    const size_t total = hdr_len + msg->length;
    if (out_cap < total)
        return PROTO_ERR_NOSPACE;

    size_t o = 0;
    /* magic */
    uint32_t magic = le32(PROTO_MAGIC);
    out[o++] = (uint8_t)(magic & 0xFF);
    out[o++] = (uint8_t)((magic >> 8) & 0xFF);
    out[o++] = (uint8_t)((magic >> 16) & 0xFF);
    out[o++] = (uint8_t)((magic >> 24) & 0xFF);
    /* header fields */
    out[o++] = PROTO_VERSION;
    out[o++] = msg->flags;
    uint16_t cmd_le = le16(msg->cmd);
    out[o++] = (uint8_t)(cmd_le & 0xFF);
    out[o++] = (uint8_t)((cmd_le >> 8) & 0xFF);
    uint32_t len_le = le32(msg->length);
    out[o++] = (uint8_t)(len_le & 0xFF);
    out[o++] = (uint8_t)((len_le >> 8) & 0xFF);
    out[o++] = (uint8_t)((len_le >> 16) & 0xFF);
    out[o++] = (uint8_t)((len_le >> 24) & 0xFF);
    /* CRC32 of header (without magic) + payload */
    /* CRC-32 over header (sans magic) + payload; IEEE 802.3 style */
    uint32_t crc = 0xFFFFFFFFu;
    crc = crc32_update(crc, &out[4], 1 + 1 + 2 + 4);
    if (msg->length && msg->payload)
    {
        crc = crc32_update(crc, msg->payload, msg->length);
    }
    uint32_t crc_le = le32(crc32_finalize(crc));
    out[o++] = (uint8_t)(crc_le & 0xFF);
    out[o++] = (uint8_t)((crc_le >> 8) & 0xFF);
    out[o++] = (uint8_t)((crc_le >> 16) & 0xFF);
    out[o++] = (uint8_t)((crc_le >> 24) & 0xFF);
    /* payload */
    if (msg->length && msg->payload)
    {
        for (uint32_t i = 0; i < msg->length; ++i)
        {
            out[o++] = msg->payload[i];
        }
    }
    if (out_len)
        *out_len = o;
    return PROTO_OK;
}

void proto_stream_init(proto_stream_t* s)
{
    s->state = 0;
    s->magic_acc = 0;
    s->hdr_pos = 0;
    s->payload_len = 0;
    s->payload_pos = 0;
    s->crc_expect = 0;
    s->crc_acc = 0;
}

static inline uint32_t rd_le32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint16_t rd_le16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

void proto_stream_feed(proto_stream_t* s, const uint8_t* data, size_t len, proto_on_msg_fn cb,
                       void* user)
{
    size_t i = 0;
    while (i < len)
    {
        uint8_t b = data[i++];
        switch (s->state)
        {
        case 0: /* find magic */
            s->magic_acc = (s->magic_acc >> 8) | ((uint32_t)b << 24);
            if (s->magic_acc == PROTO_MAGIC)
            {
                s->state = 1;
                s->hdr_pos = 0;
            }
            break;
        case 1: /* read header */
            s->hdr[s->hdr_pos++] = b;
            if (s->hdr_pos == sizeof(s->hdr))
            {
                /* debug: header complete */
                uint8_t version = s->hdr[0];
                if (version != PROTO_VERSION)
                {
                    s->state = 0; /* resync */
                    break;
                }
                s->payload_len = rd_le32(&s->hdr[4]);
                s->payload_pos = 0;
                s->crc_expect = rd_le32(&s->hdr[8]);
                /* initialize CRC accumulator with header (sans magic) */
                s->crc_acc = 0xFFFFFFFFu;
                s->crc_acc = crc32_update(s->crc_acc, s->hdr, 1 + 1 + 2 + 4);
                if (s->payload_len == 0)
                {
                    /* Zero-length payload: verify CRC and dispatch immediately */
                    uint32_t crc_calc = crc32_finalize(s->crc_acc);
                    if (crc_calc != s->crc_expect)
                    {
                        /* CRC mismatch: drop frame and resync */
                        s->state = 0;
                        s->magic_acc = 0;
                        break;
                    }
                    proto_msg_t msg = {0};
                    msg.flags = s->hdr[1];
                    msg.cmd = rd_le16(&s->hdr[2]);
                    msg.length = 0;
                    msg.payload = NULL;
                    if (cb)
                        cb(&msg, user);
                    s->state = 0; /* ready for next frame */
                    s->magic_acc = 0;
                }
                else
                {
                    s->state = 2;
                }
            }
            break;
        case 2:
        { /* payload */
            static uint8_t pbuf[1024];
            if (s->payload_pos < sizeof(pbuf))
            {
                pbuf[s->payload_pos] = b;
            }
            s->payload_pos++;
            s->crc_acc = crc32_update(s->crc_acc, &b, 1);
            if (s->payload_pos == s->payload_len)
            {
                /* Validate CRC; only dispatch on match */
                uint32_t crc_calc = crc32_finalize(s->crc_acc);
                if (crc_calc != s->crc_expect)
                {
                    s->state = 0;
                    s->magic_acc = 0;
                    break;
                }
                proto_msg_t msg = {0};
                msg.flags = s->hdr[1];
                msg.cmd = rd_le16(&s->hdr[2]);
                msg.length = s->payload_len;
                msg.payload = s->payload_len <= sizeof(pbuf) ? pbuf : NULL;
                if (cb && msg.payload)
                    cb(&msg, user);
                s->state = 0;
                s->magic_acc = 0;
            }
            break;
        }
        default:
            s->state = 0;
            break;
        }
    }
}
