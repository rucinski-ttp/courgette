import struct
import time

MAGIC = 0x1742DEC0
VERSION = 1
FLAG_RESPONSE = 0x01
FLAG_HAS_PAYLOAD = 0x02
CMD_LOG = 0x00FE


def _crc32_std(data: bytes) -> int:
    # IEEE 802.3 style (init=0xFFFFFFFF, final XOR)
    import zlib
    return (zlib.crc32(data, 0xFFFFFFFF) ^ 0xFFFFFFFF) & 0xFFFFFFFF


def encode(cmd: int, payload: bytes | None) -> bytes:
    payload = payload or b""
    flags = FLAG_HAS_PAYLOAD if payload else 0
    header_wo_magic = struct.pack('<BBHI', VERSION, flags, cmd, len(payload))
    c = _crc32_std(header_wo_magic + payload)
    header = struct.pack('<I', MAGIC) + header_wo_magic + struct.pack('<I', c)
    return header + payload


def decode_stream(buf: bytes) -> list[tuple[int, int, bytes]]:
    out: list[tuple[int, int, bytes]] = []
    i = 0
    while i + 16 <= len(buf):
        if buf[i:i+4] != struct.pack('<I', MAGIC):
            i += 1
            continue
        ver, flags, cmd, length = struct.unpack('<BBHI', buf[i+4:i+12])
        if ver != VERSION:
            i += 1
            continue
        end = i + 16 + length
        if end > len(buf):
            break
        crc_rx = struct.unpack('<I', buf[i+12:i+16])[0]
        payload = buf[i+16:end]
        # Strict CRC check: drop the frame if mismatch and resync by one byte
        calc = _crc32_std(struct.pack('<BBHI', ver, flags, cmd, length) + payload)
        if calc != crc_rx:
            i += 1
            continue
        out.append((cmd, flags, payload))
        i = end
    return out


def send_cmd(ser, cmd: int, payload: bytes | None, timeout: float = 2.0, *, empty_ok: bool = False, retries: int = 1, retry_sleep: float = 0.2):
    """Send a command and wait for a response. Optionally retry once or twice.

    Retries help when the device is still bringing up subsystems (e.g., SD card
    mount) and the first request races with initialization.
    """
    attempt = 0
    last_exc: Exception | None = None
    while attempt <= retries:
        frame = encode(cmd, payload)
        ser.write(frame)
        ser.flush()
        deadline = time.time() + timeout
        buf = b""
        while time.time() < deadline:
            buf += ser.read(256)
            msgs = decode_stream(buf)
            for c, flags, pl in msgs:
                if c == cmd and (flags & FLAG_RESPONSE):
                    if empty_ok or (pl and len(pl) > 0):
                        return pl
        last_exc = AssertionError("No response frame received for command 0x%04x" % cmd)
        attempt += 1
        if attempt <= retries:
            time.sleep(retry_sleep)
    raise last_exc  # type: ignore[misc]


def send_cmd_no_response(ser, cmd: int, payload: bytes | None):
    frame = encode(cmd, payload)
    ser.write(frame)
    ser.flush()
