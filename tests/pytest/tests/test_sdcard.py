import os
import zlib
from fixtures.sdcard import SdClient


def test_sdcard_basic(serial_conn):
    sd = SdClient(serial_conn)
    # Skip if no SD card available
    if sd.status() != 0:
        import pytest
        pytest.skip("No SD card detected/mountable")
    # Format (best-effort; ignore response payload)
    sd.format(timeout=10.0)

    # Ensure mount root lists (may be empty)
    entries = sd.list("/")
    assert isinstance(entries, list)

    # Create directory and file
    sd.mkdir("/tst")
    content = (b"Hello SD Card!\n" * 32)
    # write in two chunks
    sd.write("/tst/hello.txt", 0, content[:128])
    sd.write("/tst/hello.txt", 128, content[128:])

    size, is_dir = sd.stat("/tst/hello.txt")
    assert not is_dir and size == len(content)

    # read back in chunks
    a = sd.read("/tst/hello.txt", 0, 200)
    b = sd.read("/tst/hello.txt", 200, len(content) - 200)
    assert a + b == content

    # checksum
    crc_fw = sd.checksum("/tst/hello.txt")
    assert crc_fw == zlib.crc32(content) & 0xFFFFFFFF

    # list directory
    names = [n for n, _ in sd.list("/tst")]
    assert "hello.txt" in names

    # rename
    sd.rename("/tst/hello.txt", "/tst/hello2.txt")
    names2 = [n for n, _ in sd.list("/tst")]
    assert "hello2.txt" in names2

    # delete
    sd.delete("/tst/hello2.txt")
    names3 = [n for n, _ in sd.list("/tst")]
    assert "hello2.txt" not in names3
