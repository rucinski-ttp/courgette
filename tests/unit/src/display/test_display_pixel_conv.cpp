#include <gtest/gtest.h>

extern "C"
{
#include "display/pixel.h"
}

TEST(DisplayPixel, Rgb888ToRgb565)
{
    // Pure red
    EXPECT_EQ(pixel_rgb888_to_rgb565(255, 0, 0), 0xF800);
    // Pure green
    EXPECT_EQ(pixel_rgb888_to_rgb565(0, 255, 0), 0x07E0);
    // Pure blue
    EXPECT_EQ(pixel_rgb888_to_rgb565(0, 0, 255), 0x001F);
    // Greys
    EXPECT_EQ(pixel_rgb888_to_rgb565(0x20, 0x20, 0x20), 0x2104);
    EXPECT_EQ(pixel_rgb888_to_rgb565(0x80, 0x80, 0x80), 0x8410);
}
