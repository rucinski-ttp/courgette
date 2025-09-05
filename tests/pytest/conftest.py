import os
import glob
import time
from contextlib import contextmanager

import pytest

try:
    import serial  # type: ignore
except Exception as e:
    serial = None


def _auto_detect_port():
    by_id = "/dev/serial/by-id"
    if os.path.isdir(by_id):
        for p in sorted(glob.glob(os.path.join(by_id, "*"))):
            if any(k in p.lower() for k in ["st", "stmicro", "stm32", "st-link"]):
                try:
                    return os.path.realpath(p)
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


@pytest.fixture(scope="session")
def serial_port(pytestconfig):
    port = pytestconfig.getoption("--port") or os.environ.get("ZEPHYR_SERIAL") or _auto_detect_port()
    if port is None:
        pytest.skip("No serial port detected; set --port or ZEPHYR_SERIAL")
    return port


@pytest.fixture()
def serial_conn(serial_port, pytestconfig):
    if serial is None:
        pytest.skip("pyserial not installed")
    baud = int(pytestconfig.getoption("--baud"))
    ser = serial.Serial(serial_port, baudrate=baud, timeout=0.25)
    # Allow the board to reset/settle
    time.sleep(0.5)
    # Flush any buffered junk
    ser.reset_input_buffer()
    yield ser
    try:
        ser.close()
    except Exception:
        pass

