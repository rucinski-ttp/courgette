#include <gtest/gtest.h>

extern "C"
{
#include "protocol/protocol.h"
}

// Verify the stream parser ignores noise and re-synchronizes on MAGIC,
// properly decoding frames that follow.
TEST(Protocol, StreamResyncWithNoise)
{
    // Build two small frames
    uint8_t pl1[] = {0xAA, 0xBB};
    proto_msg_t m1{.cmd = 0x0101, .flags = 0x02, .payload = pl1, .length = sizeof(pl1)};
    uint8_t b1[64];
    size_t n1 = 0;
    ASSERT_EQ(PROTO_OK, proto_encode(&m1, b1, sizeof(b1), &n1));

    uint8_t pl2[] = {0xCC};
    proto_msg_t m2{.cmd = 0x0102, .flags = 0x02, .payload = pl2, .length = sizeof(pl2)};
    uint8_t b2[64];
    size_t n2 = 0;
    ASSERT_EQ(PROTO_OK, proto_encode(&m2, b2, sizeof(b2), &n2));

    // Noise + partial frame + noise + full frames
    std::vector<uint8_t> stream;
    // random noise
    for (int i = 0; i < 7; i++)
        stream.push_back((uint8_t)(0x30 + i));
    // first frame split by noise in the middle
    stream.insert(stream.end(), b1, b1 + 5);
    for (int i = 0; i < 3; i++)
        stream.push_back(0xFF);
    stream.insert(stream.end(), b1 + 5, b1 + n1);
    // another noise burst
    for (int i = 0; i < 4; i++)
        stream.push_back(0x00);
    // second frame
    stream.insert(stream.end(), b2, b2 + n2);

    proto_stream_t s;
    proto_stream_init(&s);
    size_t got = 0;
    auto cb = [](const proto_msg_t* m, void* u)
    {
        size_t* g = static_cast<size_t*>(u);
        if (*g == 0)
        {
            EXPECT_EQ(m->cmd, 0x0101);
            ASSERT_EQ(m->length, (uint32_t)2);
            EXPECT_EQ(m->payload[0], 0xAA);
            EXPECT_EQ(m->payload[1], 0xBB);
        }
        else if (*g == 1)
        {
            EXPECT_EQ(m->cmd, 0x0102);
            ASSERT_EQ(m->length, (uint32_t)1);
            EXPECT_EQ(m->payload[0], 0xCC);
        }
        (*g)++;
    };

    // Feed byte-by-byte as UART would
    for (auto ch : stream)
    {
        proto_stream_feed(&s, &ch, 1, cb, &got);
    }
    EXPECT_EQ(got, (size_t)2);
}
