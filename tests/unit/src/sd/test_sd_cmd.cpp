#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

extern "C"
{
#include "cmd/dispatch.h"
    int cmd_sd_init(void);
}

extern "C"
{
#include "util/crc32.h"
}

static std::vector<uint8_t> make_cstr(const std::string& s)
{
    std::vector<uint8_t> v(s.begin(), s.end());
    v.push_back(0);
    return v;
}

TEST(SD_Cmd, WriteReadChecksum)
{
    setenv("SD_HOST_ROOT", "sd_host_root_test", 1);
    ASSERT_EQ(0, cmd_sd_init());

    // mkdir /ind
    {
        auto pl = make_cstr("/ind");
        uint8_t rsp[4];
        uint32_t rlen = sizeof(rsp);
        EXPECT_EQ(0, cmd_dispatch(0x0206, pl.data(), pl.size(), rsp, &rlen));
    }

    // write /ind/one.txt in two parts
    const std::string path = "/ind/one.txt";
    std::string data;
    for (int i = 0; i < 64; i++)
        data += "abc123\n";

    // helper to build write payload
    auto write_chunk = [&](uint32_t off, const uint8_t* bytes, uint32_t len)
    {
        auto p = make_cstr(path);
        uint32_t le_off = off;
        uint32_t le_len = len;
        p.insert(p.end(), (uint8_t*)&le_off, (uint8_t*)&le_off + 4);
        p.insert(p.end(), (uint8_t*)&le_len, (uint8_t*)&le_len + 4);
        p.insert(p.end(), bytes, bytes + len);
        uint8_t rsp[1];
        uint32_t rl = sizeof(rsp);
        EXPECT_EQ(0, cmd_dispatch(0x0203, p.data(), p.size(), rsp, &rl));
    };
    write_chunk(0, (const uint8_t*)data.data(), 100);
    write_chunk(100, (const uint8_t*)data.data() + 100, (uint32_t)(data.size() - 100));

    // stat
    {
        auto p = make_cstr(path);
        uint8_t rsp[8];
        uint32_t rl = sizeof(rsp);
        ASSERT_EQ(0, cmd_dispatch(0x0207, p.data(), p.size(), rsp, &rl));
        ASSERT_EQ(rl, (uint32_t)8);
        uint32_t size = (uint32_t)rsp[0] | ((uint32_t)rsp[1] << 8) | ((uint32_t)rsp[2] << 16) |
                        ((uint32_t)rsp[3] << 24);
        uint32_t flags = (uint32_t)rsp[4] | ((uint32_t)rsp[5] << 8) | ((uint32_t)rsp[6] << 16) |
                         ((uint32_t)rsp[7] << 24);
        EXPECT_EQ(size, (uint32_t)data.size());
        EXPECT_EQ(flags & 1u, 0u);
    }

    // read in two parts and compare
    auto read_chunk = [&](uint32_t off, uint32_t len, std::vector<uint8_t>& out)
    {
        auto p = make_cstr(path);
        uint32_t le_off = off;
        uint32_t le_len = len;
        p.insert(p.end(), (uint8_t*)&le_off, (uint8_t*)&le_off + 4);
        p.insert(p.end(), (uint8_t*)&le_len, (uint8_t*)&le_len + 4);
        std::vector<uint8_t> rsp(len);
        uint32_t rl = len;
        ASSERT_EQ(0, cmd_dispatch(0x0202, p.data(), p.size(), rsp.data(), &rl));
        out.insert(out.end(), rsp.begin(), rsp.begin() + rl);
    };
    std::vector<uint8_t> acc;
    read_chunk(0, 128, acc);
    read_chunk(128, (uint32_t)(data.size() - 128), acc);
    ASSERT_EQ(acc.size(), data.size());
    EXPECT_EQ(0, std::memcmp(acc.data(), data.data(), data.size()));

    // checksum
    {
        auto p = make_cstr(path);
        uint8_t rsp[4];
        uint32_t rl = sizeof(rsp);
        ASSERT_EQ(0, cmd_dispatch(0x0208, p.data(), p.size(), rsp, &rl));
        uint32_t crc = (uint32_t)rsp[0] | ((uint32_t)rsp[1] << 8) | ((uint32_t)rsp[2] << 16) |
                       ((uint32_t)rsp[3] << 24);
        // Compute expected CRC32 using same util function on host
        uint32_t c = 0xFFFFFFFFu;
        c = crc32_update(c, data.data(), data.size());
        c = crc32_finalize(c);
        EXPECT_EQ(crc, c);
    }
}
