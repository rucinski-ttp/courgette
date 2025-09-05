#pragma once

#include <stddef.h>
#include <stdint.h>

/* Placeholder protocol API for robust framed transfers. */

typedef enum
{
    PROTO_OK = 0,
    PROTO_ERR = -1
} proto_rc_t;

typedef struct
{
    uint32_t version;
} proto_ctx_t;

void proto_init(proto_ctx_t* ctx);
proto_rc_t proto_encode(const uint8_t* payload, size_t len, uint8_t* out, size_t out_cap,
                        size_t* out_len);
