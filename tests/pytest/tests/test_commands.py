from fixtures.protocol import send_cmd, send_cmd_no_response
from fixtures.serial import serial_conn

from fixtures.protocol import FLAG_RESPONSE

# Command IDs must match firmware
CMD_ECHO = 0x0001
CMD_VERSION = 0x0002
CMD_REBOOT = 0x0003
CMD_MEM_READ = 0x0100
CMD_MEM_WRITE = 0x0101


def test_echo(serial_conn):
    payload = b"hello"
    rsp = send_cmd(serial_conn, CMD_ECHO, payload)
    assert rsp == payload


def test_version(serial_conn):
    rsp = send_cmd(serial_conn, CMD_VERSION, None)
    assert len(rsp) >= 4
    # Should be ascii hex-ish
    assert rsp.decode('ascii').strip() != ""


def test_reboot(serial_conn):
    # Send reboot (no response expected) and expect boot banner
    send_cmd_no_response(serial_conn, CMD_REBOOT, None)
    # After reboot, wait for boot banner
    data = b""
    for _ in range(80):
        data += serial_conn.read(256)
        if b"[BOOT]" in data:
            return
    assert False, "Did not observe reboot boot banner"


def test_mem_read_flash(serial_conn):
    # Read first 16 bytes of flash (vector table)
    import struct
    addr = 0x08000000
    length = 16
    payload = struct.pack('<II', addr, length)
    rsp = send_cmd(serial_conn, CMD_MEM_READ, payload)
    assert isinstance(rsp, (bytes, bytearray)) and len(rsp) == length
    # Expect first word to be a plausible stack pointer (SRAM address range)
    sp = struct.unpack('<I', rsp[:4])[0]
    assert (0x20000000 <= sp <= 0x2003FFFF) or (0x24000000 <= sp <= 0x2407FFFF)
