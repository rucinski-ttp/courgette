#include "protocol/protocol.h"

void proto_init(proto_ctx_t* ctx)
{
    if (ctx)
    {
        ctx->version = 1u;
    }
}

proto_rc_t proto_encode(const uint8_t* payload, size_t len, uint8_t* out, size_t out_cap,
                        size_t* out_len)
{
    /* Minimal placeholder: copy if it fits. Real framing comes later. */
    if (len > out_cap)
    {
        return PROTO_ERR;
    }
    for (size_t i = 0; i < len; ++i)
    {
        out[i] = payload[i];
    }
    if (out_len)
    {
        *out_len = len;
    }
    return PROTO_OK;
}
