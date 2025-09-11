#!/usr/bin/env python3
import sys
import time
from pathlib import Path

sys.path.append(str(Path(__file__).resolve().parent.parent / "tests/pytest"))
from fixtures.serial import _auto_detect_port  # type: ignore
from fixtures.display import DisplayClient  # type: ignore


def main():
    try:
        import serial  # type: ignore
    except Exception:
        print("pyserial not installed. Activate venv or pip install pyserial.", file=sys.stderr)
        return 2

    port = None
    if len(sys.argv) >= 2:
        port = sys.argv[1]
    if not port:
        port = _auto_detect_port()
    if not port:
        print("Could not detect serial port. Pass as arg or set ZEPHYR_SERIAL.", file=sys.stderr)
        return 2
    print(f"[manual] Connecting to {port} @115200 ...")
    ser = serial.Serial(port, baudrate=115200, timeout=0.7)
    time.sleep(0.4)
    ser.reset_input_buffer()

    disp = DisplayClient(ser)
    info = disp.info()
    if not info:
        print("[manual] Display info not available.", file=sys.stderr)
        return 1
    print(f"[manual] Resolution: {info['w']}x{info['h']} fmt={info['fmt']}")

    steps = [
        (255, 0, 0, "Solid RED"),
        (0, 255, 0, "Solid GREEN"),
        (0, 0, 255, "Solid BLUE"),
        (0, 0, 0, "Solid BLACK"),
        (255, 255, 255, "Solid WHITE"),
    ]

    for (r, g, b, label) in steps:
        print(f"[manual] Filling: {label} ...")
        disp.fill(r, g, b)
        time.sleep(0.25)
        ans = input(f"[manual] Is the screen showing {label}? (y/n) ")
        if ans.strip().lower() != "y":
            print("[manual] FAIL: user reported mismatch.")
            return 1
    print("[manual] PASS: All steps confirmed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

