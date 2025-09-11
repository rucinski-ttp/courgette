Display Bring-up (STM32H747I-DISCO + MB1166 Shield)
===================================================

This repo uses Zephyr's LTDC + MIPI DSI panel via the STM shield.

Important: MB1166 A09 vs A03

- A09 (newer): uses NT35510 panel. Use shield: `st_b_lcd40_dsi1_mb1166_a09`.
- A03 (older): uses OTM8009A panel. Use shield: `st_b_lcd40_dsi1_mb1166`.

This branch defaults scripts to the A09 shield.

Build and flash with the display shield enabled:

- Env: `BOARD=stm32h747i_disco_m7`
- Shield (A09 default): `SHIELD=st_b_lcd40_dsi1_mb1166_a09`

Commands:

- Build: `SHIELD=st_b_lcd40_dsi1_mb1166_a09 scripts/build.sh`
- Flash: `scripts/flash.sh`
- Serial monitor: `scripts/monitor.sh`

Integration tests:

- Automated: `scripts/run_integration_tests` (defaults to A09 shield; verifies solid color fill and optional readback)
- Manual: `scripts/run_manual_integration_tests` (walks through RED/GREEN/BLUE/BLACK/WHITE; prompts for Y/N)

Quick CLI:

- `tools/display_cli.py` provides simple commands to query info, fill colors, cycle a palette, read panel ID, and blank on/off. Example:
  - `./tools/display_cli.py info`
  - `./tools/display_cli.py fill red`
  - `./tools/display_cli.py cycle --loops 2 --delay 0.3`

Notes:

- Zephyr display is selected via `chosen.zephyr,display = &ltdc` as provided by the shield overlay.
- Panel timings, PLLs, and MIPI DSI host config are provided by the shield's board overlay for `stm32h747i_disco_m7`.
- Pixel format is preferred as RGB565 for throughput; ARGB8888/RGB888 are supported fallbacks.
- Optional: The integration test will try to read the panel ID via MIPI DCS 0x04; this may return empty on some panel revisions and is treated as optional.

API for game integration (C): see `app/include/app/display.h`.

Doom scaffolding:

- A minimal Doom task is scaffolded (`app/src/app/doom_task.c`) with command handlers in `app/src/app/cmd_doom.c`.
- Commands:
  - `CMD_ID_DOOM_START` (0x0400): start background 35 Hz tick loop.
  - `CMD_ID_DOOM_STATUS` (0x0401): query state and tick counter.
  - `CMD_ID_DOOM_STOP` (0x0402): stop the loop.
- Rendering is not wired yet; TODO is left in `doom_task.c` for integrating doomgeneric to render into the display APIs.
