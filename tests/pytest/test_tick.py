import time


def test_tick_message(serial_conn):
    deadline = time.time() + 10.0
    buf = b""
    while time.time() < deadline:
        data = serial_conn.read(256)
        if data:
            buf += data
            if b"[tick]" in buf:
                return
    assert False, f"Did not observe [tick] in serial output. Got: {buf[:200]!r}"

