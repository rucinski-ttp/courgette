import struct
from .protocol import send_cmd

CMD_SD_FORMAT = 0x0200
CMD_SD_LIST = 0x0201
CMD_SD_READ = 0x0202
CMD_SD_WRITE = 0x0203
CMD_SD_RENAME = 0x0204
CMD_SD_DELETE = 0x0205
CMD_SD_MKDIR = 0x0206
CMD_SD_STAT = 0x0207
CMD_SD_CHECKSUM = 0x0208
CMD_SD_STATUS = 0x0209


class SdClient:
    def __init__(self, ser):
        self.ser = ser

    def format(self, timeout=8.0):
        return send_cmd(self.ser, CMD_SD_FORMAT, None, timeout=timeout)

    def _cstr(self, s: str) -> bytes:
        return s.encode("utf-8") + b"\x00"

    def list(self, path: str = "/") -> list[tuple[str, int | None]]:
        payload = self._cstr(path)
        data = send_cmd(self.ser, CMD_SD_LIST, payload, timeout=3.0)
        out = []
        for line in data.splitlines():
            try:
                kind, rest = chr(line[0]), line[2:]
                if kind == "D":
                    out.append((rest.decode("utf-8"), None))
                else:
                    name, size = rest.decode("utf-8").rsplit(" ", 1)
                    out.append((name, int(size)))
            except Exception:
                pass
        return out

    def read(self, path: str, offset: int, length: int, timeout=2.0) -> bytes:
        payload = self._cstr(path) + struct.pack("<II", offset, length)
        return send_cmd(self.ser, CMD_SD_READ, payload, timeout=timeout)

    def write(self, path: str, offset: int, data: bytes, timeout=3.0) -> None:
        pl = self._cstr(path) + struct.pack("<II", offset, len(data)) + data
        send_cmd(self.ser, CMD_SD_WRITE, pl, timeout=timeout)

    def rename(self, old: str, new: str):
        pl = self._cstr(old) + self._cstr(new)
        send_cmd(self.ser, CMD_SD_RENAME, pl, timeout=2.0)

    def delete(self, path: str):
        pl = self._cstr(path)
        send_cmd(self.ser, CMD_SD_DELETE, pl, timeout=2.0)

    def mkdir(self, path: str):
        pl = self._cstr(path)
        send_cmd(self.ser, CMD_SD_MKDIR, pl, timeout=2.0)

    def stat(self, path: str) -> tuple[int, bool]:
        pl = self._cstr(path)
        data = send_cmd(self.ser, CMD_SD_STAT, pl, timeout=2.0)
        size, flags = struct.unpack("<II", data[:8])
        return size, bool(flags & 1)

    def checksum(self, path: str) -> int:
        pl = self._cstr(path)
        data = send_cmd(self.ser, CMD_SD_CHECKSUM, pl, timeout=3.0)
        return struct.unpack("<I", data)[0]

    def status(self) -> int:
        data = send_cmd(self.ser, CMD_SD_STATUS, None, timeout=2.0)
        if not data or len(data) < 4:
            return -9999
        return struct.unpack("<I", data[:4])[0]
