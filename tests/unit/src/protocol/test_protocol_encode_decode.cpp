#include <gtest/gtest.h>

extern "C"
{
#include "protocol/protocol.h"
#include "util/crc32.h"
}

TEST(Protocol, EncodeDecode)
{
    proto_ctx_t ctx;
    proto_init(&ctx);
    uint8_t payload[] = {0x10, 0x20, 0x30};
    proto_msg_t msg{.cmd = 0x1234, .flags = 0x02, .payload = payload, .length = sizeof(payload)};
    uint8_t buf[64];
    size_t out_len = 0;
    ASSERT_EQ(PROTO_OK, proto_encode(&msg, buf, sizeof(buf), &out_len));
    ASSERT_GT(out_len, (size_t)0);

    // feed back into stream parser
    size_t got = 0;
    proto_stream_t s;
    proto_stream_init(&s);
    auto cb = [](const proto_msg_t* m, void* u)
    {
        size_t* g = static_cast<size_t*>(u);
        EXPECT_EQ(m->cmd, 0x1234);
        ASSERT_EQ(m->length, (uint32_t)3);
        EXPECT_EQ(m->payload[0], 0x10);
        EXPECT_EQ(m->payload[1], 0x20);
        EXPECT_EQ(m->payload[2], 0x30);
        *g += 1;
    };
    proto_stream_feed(&s, buf, out_len, cb, &got);
    EXPECT_EQ(got, (size_t)1);
}
