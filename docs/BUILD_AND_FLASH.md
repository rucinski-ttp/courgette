# Build and Flash

Build

- Default board is `stm32h747i_disco/stm32h747xx/m7` (Zephyr new-style). Older Zephyr may also accept `stm32h747i_disco_m7`.
- Toolchain: Zephyr SDK (`ZEPHYR_SDK_INSTALL_DIR`) or GNU Arm Embedded.

```
./scripts/setup.sh
# With display shield (MB1166) on the DISCO board:
# - A09 (NT35510):
SHIELD=st_b_lcd40_dsi1_mb1166_a09 BOARD=stm32h747i_disco/stm32h747xx/m7 ./scripts/build.sh
# - A03 (OTM8009A):
# SHIELD=st_b_lcd40_dsi1_mb1166 BOARD=stm32h747i_disco/stm32h747xx/m7 ./scripts/build.sh
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
