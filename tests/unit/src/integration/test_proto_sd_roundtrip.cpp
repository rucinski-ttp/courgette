#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <tuple>
#include <vector>

extern "C"
{
#include "cmd/dispatch.h"
#include "protocol/protocol.h"
    int cmd_sd_init(void);
    int cmd_echo_init(void);
    int cmd_version_init(void);
    int cmd_mem_init(void);
}

// Forward-declare local decode helper
static std::vector<std::tuple<uint16_t, uint8_t, std::vector<uint8_t>>>
decode_stream(const std::vector<uint8_t>&);

// A minimal app-level on_msg that mirrors firmware proto_task's behavior:
// dispatch -> always respond with payload or 4-byte error code; encode via protocol.
static void on_msg_dispatch(const proto_msg_t* msg, void* user)
{
    std::vector<uint8_t>* out_frames = static_cast<std::vector<uint8_t>*>(user);
    uint8_t rsp[1024];
    uint32_t rsp_len = sizeof(rsp);
    int rc = cmd_dispatch(msg->cmd, msg->payload, msg->length, rsp, &rsp_len);
    uint8_t frame[1400];
    size_t fl = 0;
    proto_msg_t m{};
    m.cmd = msg->cmd;
    if (rc == 0)
    {
        m.flags = 0x01 | (rsp_len ? 0x02 : 0x00);
        m.payload = (rsp_len ? rsp : nullptr);
        m.length = rsp_len;
    }
    else
    {
        uint8_t err[4];
        err[0] = (uint8_t)(rc & 0xFF);
        err[1] = (uint8_t)((rc >> 8) & 0xFF);
        err[2] = (uint8_t)((rc >> 16) & 0xFF);
        err[3] = (uint8_t)((rc >> 24) & 0xFF);
        m.flags = 0x01 | 0x02;
        m.payload = err;
        m.length = 4;
    }
    ASSERT_EQ(PROTO_OK, proto_encode(&m, frame, sizeof(frame), &fl));
    out_frames->insert(out_frames->end(), frame, frame + fl);
}

static std::vector<uint8_t> send_cmd(uint16_t cmd, const std::vector<uint8_t>& payload,
                                     std::vector<uint8_t>& dev_out, proto_stream_t& dev_stream)
{
    // Host encodes a request
    proto_msg_t req{.cmd = cmd,
                    .flags = (uint8_t)(payload.empty() ? 0x00 : 0x02),
                    .payload = payload.data(),
                    .length = (uint32_t)payload.size()};
    uint8_t frame[1500];
    size_t fl = 0;
    if (proto_encode(&req, frame, sizeof(frame), &fl) != PROTO_OK)
    {
        ADD_FAILURE() << "proto_encode failed";
        return {};
    }

    // Deliver to device stream; the on_msg will append response frame(s) into dev_out
    proto_stream_feed(&dev_stream, frame, fl, on_msg_dispatch, &dev_out);

    // Host decodes the accumulated frames and finds the matching response
    std::vector<uint8_t> rsp_pl;
    auto msgs = ::decode_stream(std::vector<uint8_t>(dev_out.begin(), dev_out.end()));
    for (auto& tup : msgs)
    {
        uint16_t c = std::get<0>(tup);
        uint8_t flags = std::get<1>(tup);
        auto& pl = std::get<2>(tup);
        if (c == cmd && (flags & 0x01))
        {
            rsp_pl = pl;
            break;
        }
    }
    // Clear device out buffer after reading
    dev_out.clear();
    return rsp_pl;
}

// Minimal decode helper for C++ test; re-implemented here to avoid exposing C internals.
static std::vector<std::tuple<uint16_t, uint8_t, std::vector<uint8_t>>>
decode_stream(const std::vector<uint8_t>& buf)
{
    std::vector<std::tuple<uint16_t, uint8_t, std::vector<uint8_t>>> out;
    size_t i = 0;
    const uint32_t MAGIC = 0x1742DEC0u;
    while (i + 16 <= buf.size())
    {
        uint32_t m = (uint32_t)buf[i] | ((uint32_t)buf[i + 1] << 8) | ((uint32_t)buf[i + 2] << 16) |
                     ((uint32_t)buf[i + 3] << 24);
        if (m != MAGIC)
        {
            i++;
            continue;
        }
        uint8_t ver = buf[i + 4];
        uint8_t fl = buf[i + 5];
        uint16_t cmd = (uint16_t)buf[i + 6] | ((uint16_t)buf[i + 7] << 8);
        uint32_t len = (uint32_t)buf[i + 8] | ((uint32_t)buf[i + 9] << 8) |
                       ((uint32_t)buf[i + 10] << 16) | ((uint32_t)buf[i + 11] << 24);
        size_t end = i + 16 + len;
        if (end > buf.size())
            break;
        std::vector<uint8_t> pl(buf.begin() + i + 16, buf.begin() + end);
        out.emplace_back(cmd, fl, std::move(pl));
        i = end;
    }
    return out;
}

TEST(Integration, ProtoSdRoundtrip)
{
    setenv("SD_HOST_ROOT", "sd_host_roundtrip", 1);
    // Register commands as firmware does
    ASSERT_EQ(0, cmd_echo_init());
    ASSERT_EQ(0, cmd_version_init());
    // reboot not available in host build; skip
    ASSERT_EQ(0, cmd_mem_init());
    ASSERT_EQ(0, cmd_sd_init());

    // Device-side stream
    proto_stream_t dev;
    proto_stream_init(&dev);
    std::vector<uint8_t> dev_out;

    // Format
    {
        auto rsp = send_cmd(CMD_ID_SD_FORMAT, {}, dev_out, dev);
        (void)rsp; // empty ok
    }
    // mkdir /tst
    {
        std::vector<uint8_t> pl = {'/', 't', 's', 't', 0};
        auto rsp = send_cmd(CMD_ID_SD_MKDIR, pl, dev_out, dev);
        (void)rsp;
    }
    // write in two parts
    const std::string path = "/tst/hello.txt";
    std::string data(("Hello SD Card!\n"));
    data = std::string();
    for (int i = 0; i < 32; i++)
        data += "Hello SD Card!\n";

    auto make_write_pl = [&](uint32_t off, const uint8_t* b, uint32_t n)
    {
        std::vector<uint8_t> pl(path.begin(), path.end());
        pl.push_back(0);
        uint32_t loff = off, ln = n;
        pl.insert(pl.end(), (uint8_t*)&loff, (uint8_t*)&loff + 4);
        pl.insert(pl.end(), (uint8_t*)&ln, (uint8_t*)&ln + 4);
        pl.insert(pl.end(), b, b + n);
        return pl;
    };
    {
        auto pl = make_write_pl(0, (const uint8_t*)data.data(), 128);
        auto rsp = send_cmd(CMD_ID_SD_WRITE, pl, dev_out, dev);
        EXPECT_TRUE(rsp.empty());
    }
    {
        auto pl =
            make_write_pl(128, (const uint8_t*)data.data() + 128, (uint32_t)(data.size() - 128));
        auto rsp = send_cmd(CMD_ID_SD_WRITE, pl, dev_out, dev);
        EXPECT_TRUE(rsp.empty());
    }
    // stat
    {
        std::vector<uint8_t> pl(path.begin(), path.end());
        pl.push_back(0);
        auto rsp = send_cmd(CMD_ID_SD_STAT, pl, dev_out, dev);
        ASSERT_EQ(rsp.size(), (size_t)8);
        uint32_t size = (uint32_t)rsp[0] | ((uint32_t)rsp[1] << 8) | ((uint32_t)rsp[2] << 16) |
                        ((uint32_t)rsp[3] << 24);
        EXPECT_EQ(size, (uint32_t)data.size());
    }
    // read back
    auto make_read_pl = [&](uint32_t off, uint32_t n)
    {
        std::vector<uint8_t> pl(path.begin(), path.end());
        pl.push_back(0);
        uint32_t loff = off, ln = n;
        pl.insert(pl.end(), (uint8_t*)&loff, (uint8_t*)&loff + 4);
        pl.insert(pl.end(), (uint8_t*)&ln, (uint8_t*)&ln + 4);
        return pl;
    };
    std::vector<uint8_t> acc;
    {
        auto rsp = send_cmd(CMD_ID_SD_READ, make_read_pl(0, 200), dev_out, dev);
        acc.insert(acc.end(), rsp.begin(), rsp.end());
    }
    {
        auto rsp = send_cmd(CMD_ID_SD_READ, make_read_pl(200, (uint32_t)(data.size() - 200)),
                            dev_out, dev);
        acc.insert(acc.end(), rsp.begin(), rsp.end());
    }
    ASSERT_EQ(acc.size(), data.size());
    EXPECT_EQ(0, memcmp(acc.data(), data.data(), data.size()));
}
