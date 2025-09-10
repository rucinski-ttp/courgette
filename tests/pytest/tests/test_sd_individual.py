import os
import zlib
import pytest

from fixtures.sdcard import SdClient


@pytest.mark.timeout(30)
def test_sd_status_ok(serial_conn):
    sd = SdClient(serial_conn)
    rc = sd.status()
    if rc != 0:
        pytest.skip(f"SD card not mountable (status={rc})")


@pytest.mark.timeout(30)
def test_sd_format_then_list_root(serial_conn):
    sd = SdClient(serial_conn)
    if sd.status() != 0:
        pytest.skip("SD card not mountable")
    # Best-effort format (inline remount in firmware); should not hang and respond
    sd.format(timeout=10.0)
    entries = sd.list("/")
    assert isinstance(entries, list)


@pytest.mark.timeout(30)
def test_sd_mkdir_and_stat_dir(serial_conn):
    sd = SdClient(serial_conn)
    if sd.status() != 0:
        pytest.skip("SD card not mountable")
    sd.mkdir("/ind")
    size, is_dir = sd.stat("/ind")
    assert is_dir


@pytest.mark.timeout(30)
def test_sd_write_read_checksum(serial_conn):
    sd = SdClient(serial_conn)
    if sd.status() != 0:
        pytest.skip("SD card not mountable")
    path = "/ind/one.txt"
    data = (b"abc123\n" * 64)
    sd.write(path, 0, data[:100])
    sd.write(path, 100, data[100:])
    size, is_dir = sd.stat(path)
    assert not is_dir and size == len(data)
    a = sd.read(path, 0, 128)
    b = sd.read(path, 128, len(data) - 128)
    assert a + b == data
    crc = sd.checksum(path)
    assert crc == (zlib.crc32(data) & 0xFFFFFFFF)


@pytest.mark.timeout(30)
def test_sd_rename_and_delete(serial_conn):
    sd = SdClient(serial_conn)
    if sd.status() != 0:
        pytest.skip("SD card not mountable")
    a = "/ind/tmp.txt"
    b = "/ind/tmp2.txt"
    sd.write(a, 0, b"hello")
    sd.rename(a, b)
    names = [n for n, _ in sd.list("/ind")]
    assert "tmp2.txt" in names
    sd.delete(b)
    names2 = [n for n, _ in sd.list("/ind")]
    assert "tmp2.txt" not in names2

