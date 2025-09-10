import os
import zlib
import pytest
import random

from fixtures.sdcard import SdClient


def _should_run_long():
    return os.environ.get("SD_LONG") == "1"


@pytest.mark.timeout(600)
def test_sd_large_generate_and_verify(serial_conn):
    # Default to 10 MiB so it runs by default; override with SD_SIZE_MB

    sd = SdClient(serial_conn)
    if sd.status() != 0:
        pytest.skip("SD card not mountable")

    # Default 10 MiB unless SD_SIZE_MB is provided
    size_mb = int(os.environ.get("SD_SIZE_MB", "10"))
    size = size_mb * 1024 * 1024
    path = "/big/test.bin"

    # Create directory
    sd.mkdir("/big")

    # Device-side fill with deterministic pattern (no serial transfer)
    sd.fill(path, size=size, seed=0x12345678)

    # Device checksum
    crc_dev = sd.checksum(path)

    # Host expected checksum using the same PRNG (xorshift32)
    def prng_bytes(total, seed=0x12345678, chunk=1<<20):
        x = seed & 0xFFFFFFFF
        remaining = total
        while remaining > 0:
            n = min(remaining, chunk)
            block = bytearray(n)
            for i in range(n):
                x ^= (x << 13) & 0xFFFFFFFF
                x ^= (x >> 17) & 0xFFFFFFFF
                x ^= (x << 5) & 0xFFFFFFFF
                block[i] = x & 0xFF
            yield bytes(block)
            remaining -= n

    c = 0xFFFFFFFF
    for b in prng_bytes(size):
        c = zlib.crc32(b, c)
    crc_host = (c ^ 0xFFFFFFFF) & 0xFFFFFFFF

    assert crc_dev == crc_host

    # Sample a few reads and verify consistency too
    sample_cnt = int(os.environ.get("SD_SAMPLE_READS", "4"))
    sample_len = int(os.environ.get("SD_SAMPLE_LEN", str(64 * 1024)))
    rng = random.Random(123)
    for _ in range(sample_cnt):
        off = rng.randrange(0, max(1, size - sample_len))
        data = sd.read(path, off, sample_len, timeout=30.0)
        # Recompute expected sample
        x = 0x12345678 & 0xFFFFFFFF
        # advance PRNG to offset (byte-by-byte)
        for _i in range(off):
            x ^= (x << 13) & 0xFFFFFFFF; x ^= (x >> 17) & 0xFFFFFFFF; x ^= (x << 5) & 0xFFFFFFFF
        expected = bytearray(sample_len)
        for i in range(sample_len):
            x ^= (x << 13) & 0xFFFFFFFF; x ^= (x >> 17) & 0xFFFFFFFF; x ^= (x << 5) & 0xFFFFFFFF
            expected[i] = x & 0xFF
        assert data == bytes(expected)
