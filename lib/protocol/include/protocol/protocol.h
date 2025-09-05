#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PROTO_MAGIC 0x1742DEC0u
#define PROTO_VERSION 1u

    typedef struct
    {
        uint8_t version;
    } proto_ctx_t;

    typedef struct
    {
        uint16_t cmd;
        uint8_t flags;
        const uint8_t* payload;
        uint32_t length;
    } proto_msg_t;

    typedef enum
    {
        PROTO_OK = 0,
        PROTO_ERR = -1,
        PROTO_ERR_NOSPACE = -2,
    } proto_rc_t;

    /* Streaming parser state */
    typedef struct
    {
        uint8_t state;
        uint32_t magic_acc;
        uint8_t hdr[1 + 1 + 2 + 4 + 4];
        uint8_t hdr_pos;
        uint32_t payload_len;
        uint32_t payload_pos;
        uint32_t crc_expect;
        uint32_t crc_acc;
    } proto_stream_t;

    typedef void (*proto_on_msg_fn)(const proto_msg_t* msg, void* user);

    void proto_init(proto_ctx_t* ctx);
    proto_rc_t proto_encode(const proto_msg_t* msg, uint8_t* out, size_t out_cap, size_t* out_len);
    void proto_stream_init(proto_stream_t* s);
    void proto_stream_feed(proto_stream_t* s, const uint8_t* data, size_t len, proto_on_msg_fn cb,
                           void* user);

#ifdef __cplusplus
}
#endif
