/* Protocol encode/decode and streaming parser */
#include "protocol/protocol.h"
#include "util/crc32.h"
#include <string.h>

/* Optional weak debug hook. The application may override this to route
 * protocol-layer debug messages over its preferred channel (e.g., serial
 * LOG frames). If not provided, calls are no-ops. */
__attribute__((weak)) void proto_debug_log(const char* msg) { (void)msg; }

static inline uint16_t le16(uint16_t v) { return v; }
static inline uint32_t le32(uint32_t v) { return v; }

/* (Removed) Bitwise CRC-32 helper; use util/crc32.h implementation for parity with host. */

void proto_init(proto_ctx_t* ctx)
{
    if (ctx)
    {
        ctx->version = PROTO_VERSION;
    }
}

proto_rc_t proto_encode(const proto_msg_t* msg, uint8_t* out, size_t out_cap, size_t* out_len)
{
    proto_rc_t rc = PROTO_ERR;
    if (msg && out)
    {
        const size_t hdr_len = 4 /*magic*/ + 1 + 1 + 2 + 4 + 4;
        const size_t total = hdr_len + msg->length;
        if (out_cap >= total)
        {
            size_t o = 0;
            /* magic */
            uint32_t magic = le32(PROTO_MAGIC);
            out[o++] = (uint8_t)(magic & 0xFFu);
            out[o++] = (uint8_t)((magic >> 8u) & 0xFFu);
            out[o++] = (uint8_t)((magic >> 16u) & 0xFFu);
            out[o++] = (uint8_t)((magic >> 24u) & 0xFFu);
            /* header fields */
            out[o++] = PROTO_VERSION;
            out[o++] = msg->flags;
            uint16_t cmd_le = le16(msg->cmd);
            out[o++] = (uint8_t)(cmd_le & 0xFFu);
            out[o++] = (uint8_t)((cmd_le >> 8u) & 0xFFu);
            uint32_t len_le = le32(msg->length);
            out[o++] = (uint8_t)(len_le & 0xFFu);
            out[o++] = (uint8_t)((len_le >> 8u) & 0xFFu);
            out[o++] = (uint8_t)((len_le >> 16u) & 0xFFu);
            out[o++] = (uint8_t)((len_le >> 24u) & 0xFFu);
            /* CRC-32 */
            uint32_t crc = 0xFFFFFFFFu;
            crc = crc32_update(crc, &out[4], 1 + 1 + 2 + 4);
            if (msg->length && msg->payload)
            {
                crc = crc32_update(crc, msg->payload, msg->length);
            }
            uint32_t crc_le = le32(crc32_finalize(crc));
            out[o++] = (uint8_t)(crc_le & 0xFFu);
            out[o++] = (uint8_t)((crc_le >> 8u) & 0xFFu);
            out[o++] = (uint8_t)((crc_le >> 16u) & 0xFFu);
            out[o++] = (uint8_t)((crc_le >> 24u) & 0xFFu);
            /* payload */
            if (msg->length && msg->payload)
            {
                for (uint32_t i = 0; i < msg->length; ++i)
                {
                    out[o++] = msg->payload[i];
                }
            }
            if (out_len)
            {
                *out_len = o;
            }
            rc = PROTO_OK;
        }
        else
        {
            rc = PROTO_ERR_NOSPACE;
        }
    }
    return rc;
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
    s->crc_acc_alt = 0;
}

static inline uint32_t rd_le32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint16_t rd_le16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

static void dbg_append(char* dst, size_t* pos, const char* txt)
{
    if (!dst || !pos || !txt)
    {
        return;
    }
    size_t p = *pos;
    while (txt[0] != '\0' && p < sizeof(((proto_stream_t*)0)->hdr) * 4)
    {
        dst[p++] = *txt++;
    }
    *pos = p;
}

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
            /* Defer CRC accumulation until full header is available to avoid
             * any ambiguity; compute header CRC in one shot. */
            if (s->hdr_pos == sizeof(s->hdr))
            {
                /* debug: header complete */
                uint8_t version = s->hdr[0];
                {
                    /* Build a compact debug line without relying on kernel printf */
                    char dbg[96];
                    size_t p = 0;
                    const char* prefix = "[proto_hdr] ";
                    dbg_append(dbg, &p, prefix);
                    /* Append fields in simple decimal/hex */
                    const char* vstr = "v=";
                    memcpy(&dbg[p], vstr, 2);
                    p += 2;
                    dbg[p++] = (char)('0' + (version % 10));
                    const char* flstr = " fl=0x";
                    memcpy(&dbg[p], flstr, 6);
                    p += 6;
                    static const char hex[] = "0123456789abcdef";
                    dbg[p++] = hex[(s->hdr[1] >> 4) & 0xF];
                    dbg[p++] = hex[s->hdr[1] & 0xF];
                    uint16_t d_cmd = rd_le16(&s->hdr[2]);
                    uint32_t d_len = rd_le32(&s->hdr[4]);
                    uint32_t d_crc = rd_le32(&s->hdr[8]);
                    const char* cstr = " cmd=0x";
                    memcpy(&dbg[p], cstr, 7);
                    p += 7;
                    for (int sh = 12; sh >= 0; sh -= 4)
                    {
                        dbg[p++] = hex[(d_cmd >> sh) & 0xF];
                    }
                    const char* lstr = " len=";
                    memcpy(&dbg[p], lstr, 5);
                    p += 5;
                    /* append len as decimal (limited) */
                    {
                        char tmp[10];
                        int ti = 0;
                        uint32_t t = d_len;
                        if (t == 0)
                            tmp[ti++] = '0';
                        else
                        {
                            char rev[10];
                            int ri = 0;
                            while (t && ri < 10)
                            {
                                rev[ri++] = (char)('0' + (t % 10u));
                                t /= 10u;
                            }
                            while (ri)
                                tmp[ti++] = rev[--ri];
                        }
                        memcpy(&dbg[p], tmp, (size_t)ti);
                        p += (size_t)ti;
                    }
                    const char* crs = " crc=";
                    memcpy(&dbg[p], crs, 5);
                    p += 5;
                    for (int sh = 28; sh >= 0; sh -= 4)
                    {
                        dbg[p++] = hex[(d_crc >> sh) & 0xF];
                    }
                    dbg[p] = '\0';
                    proto_debug_log(dbg);
                }
                if (version != PROTO_VERSION)
                {
                    s->state = 0; /* resync */
                    break;
                }
                s->payload_len = rd_le32(&s->hdr[4]);
                s->payload_pos = 0;
                s->crc_expect = rd_le32(&s->hdr[8]);
                /* Compute CRC over header (sans magic) */
                s->crc_acc = 0xFFFFFFFFu;
                s->crc_acc = crc32_update(s->crc_acc, &s->hdr[0], 1 + 1 + 2 + 4);
                if (s->payload_len == 0)
                {
                    /* Zero-length payload: verify CRC and dispatch immediately */
                    {
                        /* debug: show pre-final accumulator */
                        char dbg[64];
                        size_t p = 0;
                        const char* pre = "[crc_pre] ";
                        while (*pre)
                        {
                            dbg[p++] = *pre++;
                        }
                        static const char hex[] = "0123456789abcdef";
                        uint32_t precrc = s->crc_acc;
                        for (unsigned sh = 28u; ; sh -= 4u)
                        {
                            dbg[p++] = hex[(precrc >> sh) & 0xF];
                            if (sh == 0u)
                                break;
                        }
                        dbg[p] = 0;
                        proto_debug_log(dbg);
                    }
                    uint32_t crc_calc_std = crc32_finalize(s->crc_acc);
                    if (crc_calc_std != s->crc_expect)
                    {
                        /* CRC mismatch: drop frame and resync (strict) */
                        char dbg[64];
                        size_t p = 0;
                        static const char hex[] = "0123456789abcdef";
                        const char* pre = "[proto_crc_mismatch] exp=";
                        dbg_append(dbg, &p, pre);
                        for (unsigned sh = 28u; ; sh -= 4u)
                        {
                            dbg[p++] = hex[(s->crc_expect >> sh) & 0xF];
                            if (sh == 0u)
                                break;
                        }
                        const char* mid = " got=";
                        dbg_append(dbg, &p, mid);
                        for (unsigned sh = 28u; ; sh -= 4u)
                        {
                            dbg[p++] = hex[(crc_calc_std >> sh) & 0xF];
                            if (sh == 0u)
                                break;
                        }
                        dbg[p] = '\0';
                        proto_debug_log(dbg);
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
                    {
                        cb(&msg, user);
                    }
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
                uint32_t crc_calc_std = crc32_finalize(s->crc_acc);
                if (crc_calc_std != s->crc_expect)
                {
                    /* CRC mismatch: drop frame and resync (strict) */
                    char dbg[64];
                    size_t p = 0;
                    static const char hex[] = "0123456789abcdef";
                    const char* pre = "[proto_crc_mismatch] exp=";
                    dbg_append(dbg, &p, pre);
                    for (unsigned sh = 28u; ; sh -= 4u)
                    {
                        dbg[p++] = hex[(s->crc_expect >> sh) & 0xF];
                        if (sh == 0u)
                            break;
                    }
                    const char* mid = " got=";
                    dbg_append(dbg, &p, mid);
                    for (unsigned sh = 28u; ; sh -= 4u)
                    {
                        dbg[p++] = hex[(crc_calc_std >> sh) & 0xF];
                        if (sh == 0u)
                            break;
                    }
                    dbg[p] = '\0';
                    proto_debug_log(dbg);
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
                {
                    cb(&msg, user);
                }
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
