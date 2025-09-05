#include <gtest/gtest.h>

extern "C"
{
#include "cmd/dispatch.h"
#include "cmd/echo.h"
#include "cmd/version.h"
}

TEST(Cmd, RegisterAndDispatch)
{
    ASSERT_EQ(0, cmd_echo_init());
    ASSERT_EQ(0, cmd_version_init());

    const uint8_t in[] = {1, 2, 3};
    uint8_t out[8] = {};
    uint32_t out_len = sizeof(out);
    ASSERT_EQ(0, cmd_dispatch(CMD_ID_ECHO, in, sizeof(in), out, &out_len));
    ASSERT_EQ(out_len, (uint32_t)3);
    EXPECT_EQ(out[0], 1);
    EXPECT_EQ(out[1], 2);
    EXPECT_EQ(out[2], 3);
}
