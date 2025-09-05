from fixtures.protocol import decode_stream, CMD_LOG
import time


def test_tick_message(serial_conn):
    deadline = time.time() + 10.0
    buf = b""
    while time.time() < deadline:
        buf += serial_conn.read(256)
        msgs = decode_stream(buf)
        for cmd, flags, pl in msgs:
            if cmd == CMD_LOG and b"[tick]" in pl:
                return
    assert False, f"Did not observe [tick] log frame. Got bytes: {buf[:200]!r}"
