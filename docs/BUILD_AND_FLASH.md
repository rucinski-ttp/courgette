# Build and Flash

Build

- Default board is `stm32h747i_disco_m7`.
- Toolchain: Zephyr SDK (`ZEPHYR_SDK_INSTALL_DIR`) or GNU Arm Embedded.

```
./scripts/setup.sh
BOARD=stm32h747i_disco_m7 ./scripts/build.sh
```

Flash

```
./scripts/flash.sh
```

Monitor

```
./scripts/monitor.sh           # auto-detects port
./scripts/monitor.sh /dev/ttyACM0 115200
```

Expected Output

- Boot line, then periodic tick once per second.

