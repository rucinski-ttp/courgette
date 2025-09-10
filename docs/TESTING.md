# Testing

Unit Tests (host)

- Uses CMake + FetchContent for GoogleTest
- Lives in `tests/unit/`

```
./scripts/test_unit.sh
```

Integration Tests (board)

- Python `pytest` harness in `tests/pytest/` drives the binary protocol end-to-end
  (echo/version/mem + full SD card operations).
- The serial fixture auto-detects the ST-LINK VCP, drains early logs, and performs a `VERSION`
  handshake. A `[READY]` / periodic `[tick]` LOG frame indicates liveness. Tests skip when the
  SD card is not present or not mountable.
- Assumes the firmware is already built and flashed.

```
./scripts/build.sh
./scripts/flash.sh
./scripts/test_integration.sh
```

Manual SD CLI

- For quick checks, use `tools/sd_cli.py` which shares the same protocol class as pytest.
- Examples:
  - `python tools/sd_cli.py status`
  - `python tools/sd_cli.py format`
  - `python tools/sd_cli.py list /`
  - `python tools/sd_cli.py write /ind/hello.txt "hello world"`
  - `python tools/sd_cli.py read /ind/hello.txt 0:64`
  - `python tools/sd_cli.py checksum /ind/hello.txt`
  - `python tools/sd_cli.py logs`
