# Testing

Unit Tests (host)

- Uses CMake + FetchContent for GoogleTest
- Lives in `tests/unit/`

```
./scripts/test_unit.sh
```

Integration Tests (board)

- Python `pytest` harness in `tests/pytest/`
- Serial fixture auto-detects ST-LINK VCP and asserts that the firmware emits
  tick messages.
- Assumes the firmware is already built and flashed.

```
./scripts/build.sh
./scripts/flash.sh
./scripts/test_integration.sh
```
