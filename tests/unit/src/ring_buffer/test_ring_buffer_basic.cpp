#include <gtest/gtest.h>

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

TEST(RingBuffer, WrapAround)
{
    uint8_t storage[8];
    rb_t rb;
    rb_init(&rb, storage, sizeof(storage));

    const uint8_t a[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(rb_write(&rb, a, sizeof(a)), sizeof(a));
    uint8_t out[3];
    EXPECT_EQ(rb_read(&rb, out, sizeof(out)), sizeof(out));
    const uint8_t b[] = {6, 7, 8, 9};
    EXPECT_EQ(rb_write(&rb, b, sizeof(b)), sizeof(b));
    uint8_t rest[8] = {};
    size_t r = rb_read(&rb, rest, sizeof(rest));
    EXPECT_EQ(r, (size_t)6);
}
