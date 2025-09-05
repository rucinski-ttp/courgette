#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

extern "C"
{
#include "util/ring_buffer.h"
}

TEST(RingBuffer, BasicReadWrite)
{
    uint8_t storage[16];
    rb_t rb;
    rb_init(&rb, storage, sizeof(storage));

    const uint8_t input[] = {1, 2, 3, 4, 5};
    auto written = rb_write(&rb, input, sizeof(input));
    EXPECT_EQ(written, sizeof(input));
    EXPECT_EQ(rb_used(&rb), sizeof(input));

    uint8_t out[8] = {};
    auto read = rb_read(&rb, out, sizeof(out));
    EXPECT_EQ(read, sizeof(input));
    for (size_t i = 0; i < sizeof(input); ++i)
    {
        EXPECT_EQ(out[i], input[i]);
    }
    EXPECT_EQ(rb_used(&rb), 0u);
}
