#!/usr/bin/env python3
import argparse
import os
import sys
import glob
import time
import serial

# Allow running from repo root by adding tests/pytest to path
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
PYTEST_DIR = os.path.join(REPO, 'tests', 'pytest')
if PYTEST_DIR not in sys.path:
    sys.path.insert(0, PYTEST_DIR)

from fixtures.sdcard import SdClient  # type: ignore
from fixtures.protocol import decode_stream, CMD_LOG  # type: ignore


def auto_port():
    by_id = "/dev/serial/by-id"
    if os.path.isdir(by_id):
        for p in sorted(glob.glob(os.path.join(by_id, "*"))):
            if any(k in p.lower() for k in ["st", "stmicro", "stm32", "st-link"]):
                return os.path.realpath(p)
    for pat in ["/dev/ttyACM*", "/dev/ttyUSB*", "/dev/cu.usbmodem*", "/dev/cu.usbserial*"]:
        m = sorted(glob.glob(pat))
        if m:
            return m[0]
    return None


def main():
    ap = argparse.ArgumentParser(description="Manual SD card operations over protocol")
    ap.add_argument("cmd", choices=["status","format","list","read","write","rename","delete","mkdir","stat","checksum","logs"], help="command")
    ap.add_argument("path", nargs="?", help="path")
    ap.add_argument("arg", nargs="?", help="additional argument")
    ap.add_argument("--port", default=None)
    ap.add_argument("--baud", default=115200, type=int)
    args = ap.parse_args()

    port = args.port or auto_port()
    if not port:
        raise SystemExit("No serial port found")
    ser = serial.Serial(port, baudrate=args.baud, timeout=0.25)
    sd = SdClient(ser)

    if args.cmd == "logs":
        end = time.time() + 5.0
        buf = b""
        while time.time() < end:
            buf += ser.read(256)
            for c, fl, pl in decode_stream(buf):
                if c == CMD_LOG:
                    try: print(pl.decode())
                    except: pass
        return

    if args.cmd == "status":
        print("status:", sd.status())
    elif args.cmd == "format":
        sd.format(timeout=10.0)
        print("format: ok")
    elif args.cmd == "list":
        p = args.path or "/"
        print(sd.list(p))
    elif args.cmd == "mkdir":
        if not args.path: raise SystemExit("mkdir requires path")
        sd.mkdir(args.path)
        print("mkdir: ok")
    elif args.cmd == "write":
        if not args.path or args.arg is None: raise SystemExit("write requires path and data")
        data = args.arg.encode("utf-8")
        sd.write(args.path, 0, data)
        print("write: ok")
    elif args.cmd == "read":
        if not args.path: raise SystemExit("read requires path")
        off = 0; length = 256
        if args.arg and ":" in args.arg:
            off, length = map(int, args.arg.split(":",1))
        print(sd.read(args.path, off, length))
    elif args.cmd == "rename":
        if not args.path or not args.arg: raise SystemExit("rename requires old and new path")
        sd.rename(args.path, args.arg)
        print("rename: ok")
    elif args.cmd == "delete":
        if not args.path: raise SystemExit("delete requires path")
        sd.delete(args.path)
        print("delete: ok")
    elif args.cmd == "stat":
        if not args.path: raise SystemExit("stat requires path")
        print(sd.stat(args.path))
    elif args.cmd == "checksum":
        if not args.path: raise SystemExit("checksum requires path")
        print(hex(sd.checksum(args.path)))

if __name__ == "__main__":
    main()
