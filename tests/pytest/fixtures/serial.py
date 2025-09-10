import os
import glob
import time
import subprocess
from pathlib import Path
import pytest

try:
    import serial  # type: ignore
except Exception:
    serial = None


def _auto_detect_port():
    by_id = "/dev/serial/by-id"
    if os.path.isdir(by_id):
        for p in sorted(glob.glob(os.path.join(by_id, "*"))):
            if any(k in p.lower() for k in ["st", "stmicro", "stm32", "st-link"]):
                try:
                    # Return the stable by-id symlink itself to survive re-enumeration
                    return p
                except Exception:
                    pass
    for pattern in ["/dev/ttyACM*", "/dev/ttyUSB*", "/dev/cu.usbmodem*", "/dev/cu.usbserial*"]:
        matches = sorted(glob.glob(pattern))
        if matches:
            return matches[0]
    return None


def pytest_addoption(parser):
    parser.addoption("--port", action="store", default=None, help="Serial port override")
    parser.addoption("--baud", action="store", default="115200", help="Serial baud rate")
    parser.addoption("--no-hard-reset", action="store_true", default=False, help="Skip hard reset between tests")


@pytest.fixture(scope="session")
def serial_port(pytestconfig):
    port = pytestconfig.getoption("--port") or os.environ.get("ZEPHYR_SERIAL") or _auto_detect_port()
    if port is None:
        pytest.fail("No serial port detected; set --port or ZEPHYR_SERIAL to proceed")
    return port


def _hard_reset(pytestconfig):
    if pytestconfig.getoption("--no-hard-reset"):
        return
    # Try OpenOCD reset-run to ensure a clean slate between tests.
    cfg = Path("zephyr/boards/arm/stm32h747i_disco/support/openocd_stm32h747i_disco_m7.cfg")
    if cfg.exists():
        try:
            subprocess.run([
                "openocd", "-f", str(cfg),
                "-c", "init; reset run; sleep 200; shutdown"
            ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=4, check=False)
            # Give USB VCP a moment to re-enumerate
            time.sleep(0.6)
        except Exception:
            pass


def _wait_ready(ser, timeout=5.0):
    # Drain a bit and look for a readiness marker so commands aren't sent too early
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        try:
            buf += ser.read(256)
            if b"[READY]" in buf or b"[cmd_reg] sd" in buf or b"[BOOT]" in buf:
                return True
        except Exception:
            break
    return False


def _try_version(ser, *, timeout=2.8, retries=3, retry_sleep=0.25) -> bool:
    try:
        from .protocol import send_cmd as _send_cmd
        CMD_VERSION = 0x0002
        _send_cmd(ser, CMD_VERSION, None, timeout=timeout, retries=retries, retry_sleep=retry_sleep)
        return True
    except Exception:
        return False


@pytest.fixture()
def serial_conn(serial_port, pytestconfig):
    if serial is None:
        pytest.skip("pyserial not installed")
    # Hard reset device to avoid cascading failures from prior tests
    _hard_reset(pytestconfig)
    baud = int(pytestconfig.getoption("--baud"))
    ser = serial.Serial(serial_port, baudrate=baud, timeout=0.6)
    # Nudge DTR/RTS to wake ST-LINK VCP on some hosts
    try:
        ser.dtr = False
        ser.rts = False
        time.sleep(0.05)
        ser.dtr = True
        ser.rts = True
    except Exception:
        pass
    time.sleep(0.5)
    ser.reset_input_buffer()
    _wait_ready(ser, timeout=5.0)
    # Handshake: try a VERSION ping to ensure dispatch is ready
    success = _try_version(ser, timeout=2.8, retries=3, retry_sleep=0.25)
    if not success:
        # Reopen port once if no response observed
        try:
            ser.close()
        except Exception:
            pass
        time.sleep(0.5)
        ser = serial.Serial(serial_port, baudrate=baud, timeout=0.6)
        try:
            ser.dtr = False; ser.rts = False; time.sleep(0.05); ser.dtr = True; ser.rts = True
        except Exception:
            pass
        time.sleep(0.5)
        ser.reset_input_buffer()
        success = _try_version(ser, timeout=3.2, retries=4, retry_sleep=0.3)
    if not success:
        pytest.fail("Device did not respond to VERSION handshake; firmware not running or protocol mismatch")
    yield ser
    try:
        ser.close()
    except Exception:
        pass
