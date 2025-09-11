import pytest

from fixtures.display import DisplayClient


@pytest.mark.timeout(60)
def test_display_info_and_fill(serial_conn):
    disp = DisplayClient(serial_conn)
    info = disp.info()
    assert info is not None
    assert info["w"] > 0 and info["h"] > 0

    # Cycle solid colors and verify readback in center area when readable
    colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]
    for (r, g, b) in colors:
        disp.fill(r, g, b)
        if not info["can_read"]:
            # If driver doesn't support readback, just continue
            continue
        # Read a small 8x8 block from the center
        cx = info["w"] // 2
        cy = info["h"] // 2
        w = h = 8
        x = max(0, cx - w // 2)
        y = max(0, cy - h // 2)
        data = disp.read_rect(x, y, w, h, timeout=5.0)
        assert isinstance(data, (bytes, bytearray))
        # Validate first pixel matches expected color according to pixel format
        fmt = info["fmt"]
        if fmt == 0:  # RGB565
            assert len(data) >= 2
            px = (data[0] << 8) | data[1]
            rr = (px >> 11) & 0x1F
            gg = (px >> 5) & 0x3F
            bb = (px >> 0) & 0x1F
            # Allow small tolerance due to quantization
            def almost(val, bits, want):
                # expand channel back to 0..255
                scale = 255.0 / ((1 << bits) - 1)
                v = int(round(val * scale))
                return abs(v - want) <= 16

            assert almost(rr, 5, r)
            assert almost(gg, 6, g)
            assert almost(bb, 5, b)
        elif fmt == 1:  # RGB888
            assert len(data) >= 3
            assert abs(data[0] - r) <= 8
            assert abs(data[1] - g) <= 8
            assert abs(data[2] - b) <= 8
        elif fmt == 2:  # ARGB8888
            assert len(data) >= 4
            assert data[0] in (0x00, 0xFF)  # alpha often 0 or 255 depending on driver
            assert abs(data[1] - r) <= 8
            assert abs(data[2] - g) <= 8
            assert abs(data[3] - b) <= 8


@pytest.mark.timeout(10)
def test_display_get_id(serial_conn):
    disp = DisplayClient(serial_conn)
    data = disp.get_id()
    # Optional: if supported, expect a few non-zero bytes
    if data:
        assert any(b != 0x00 for b in data)

