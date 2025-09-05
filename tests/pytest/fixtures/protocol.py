import struct
import time

MAGIC = 0x1742DEC0
VERSION = 1
FLAG_RESPONSE = 0x01
FLAG_HAS_PAYLOAD = 0x02
CMD_LOG = 0x00FE


def crc32(data: bytes) -> int:
    import zlib
    return zlib.crc32(data) & 0xFFFFFFFF


def encode(cmd: int, payload: bytes | None) -> bytes:
    payload = payload or b""
    flags = FLAG_HAS_PAYLOAD if payload else 0
    header_wo_magic = struct.pack('<BBHI', VERSION, flags, cmd, len(payload))
    c = crc32(header_wo_magic + payload)
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
        crc_rx = struct.unpack('<I', buf[i+12:i+16])[0]
        end = i + 16 + length
        if end > len(buf):
            break
        payload = buf[i+16:end]
        # Enforce CRC validity: header without magic is 8 bytes (version..length)
        header_wo_magic = buf[i+4:i+12]
        if crc32(header_wo_magic + payload) == crc_rx:
            out.append((cmd, flags, payload))
        i = end
    return out


def send_cmd(ser, cmd: int, payload: bytes | None, timeout: float = 2.0):
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
                return pl
    raise AssertionError("No response frame received for command 0x%04x" % cmd)


def send_cmd_no_response(ser, cmd: int, payload: bytes | None):
    frame = encode(cmd, payload)
    ser.write(frame)
    ser.flush()
