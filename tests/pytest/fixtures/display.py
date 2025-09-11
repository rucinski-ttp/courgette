from .protocol import send_cmd

CMD_DISP_INFO = 0x0300
CMD_DISP_FILL = 0x0301
CMD_DISP_READ = 0x0302
CMD_DISP_GET_ID = 0x0303


class DisplayClient:
    def __init__(self, ser):
        self.ser = ser

    def info(self):
        data = send_cmd(self.ser, CMD_DISP_INFO, None, timeout=2.0)
        if len(data) < 8:
            return None
        w = data[0] | (data[1] << 8)
        h = data[2] | (data[3] << 8)
        fmt = data[4]
        can_read = data[5] != 0
        can_write = data[6] != 0
        stride = data[8] | (data[9] << 8) if len(data) >= 10 else 0
        return {"w": w, "h": h, "fmt": fmt, "can_read": can_read, "can_write": can_write, "stride": stride}

    def fill(self, r, g, b):
        send_cmd(self.ser, CMD_DISP_FILL, bytes([r, g, b]), timeout=6.0, empty_ok=True)

    def read_rect(self, x, y, w, h, timeout=3.0):
        import struct
        pl = struct.pack('<HHHH', x, y, w, h)
        return send_cmd(self.ser, CMD_DISP_READ, pl, timeout=timeout)

    def get_id(self):
        return send_cmd(self.ser, CMD_DISP_GET_ID, None, timeout=1.0, empty_ok=True)

