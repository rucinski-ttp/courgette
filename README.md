Zephyr bring-up for STM32H747I-DISCO (M7)

This repository contains a Zephyr application for the STM32H747I-DISCO board
targeting the M7 core. It exposes a strict framed serial protocol over the
ST-LINK VCP for robust host communication and implements SD card operations
via FATFS (format, list, read/write, rename/delete, stat, checksum). A compact
LOG stream provides debug visibility without interfering with protocol parsing.

Quick start

- Prereqs: USB ST-LINK connected, board powered, and a recent Git + Python 3.
- Run `scripts/setup.sh` once to fetch Zephyr and deps into a local venv.
- Build with `scripts/build.sh`.
- Flash with `scripts/flash.sh`.
- Monitor serial output with `scripts/monitor.sh`.
- Run unit tests with `scripts/test_unit.sh`.
- Run integration tests with `scripts/test_integration.sh` (requires board; assumes flashed).
- Optional: use `tools/sd_cli.py` for manual SD operations over the protocol.

SD Card Notes

- The firmware uses Zephyr's FATFS over SDMMC1. The mount point is set dynamically to the
  disk-access path (e.g., `"/SD:"`). Command paths accept POSIX-like `/` style and are joined to
  the mount point internally.
- UART logging during SD operations is intentionally minimized to keep command responses
  non-blocking over the 115200 bps link.

Scripts

- `scripts/setup.sh`: Creates `.venv`, installs `west`, initializes Zephyr
  (v3.6.0), fetches modules, and installs Python requirements.
- `scripts/build.sh`: Builds the app for `stm32h747i_disco_m7` by default.
  Override with `BOARD=...`.
- `scripts/flash.sh`: Flashes the built image via `west flash`.
- `scripts/monitor.sh`: Opens a serial terminal to the ST-LINK VCP.

Notes

- Toolchain: This setup expects either Zephyr SDK (`ZEPHYR_SDK_INSTALL_DIR` set)
  or a working ARM GCC toolchain (`arm-none-eabi-gcc`) plus `cmake` and `ninja`.
  If missing, `scripts/setup.sh` will point you to options to install.
- Runner: Flashing uses OpenOCD runner by default for this board.
  Ensure OpenOCD is installed and accessible.

Docs

- See `docs/GETTING_STARTED.md`, `docs/BUILD_AND_FLASH.md`, `docs/TESTING.md`, `docs/ARCHITECTURE.md` for additional details.
- See `docs/CODING_STANDARDS.md` for code style and tidy policies.

Formatting & Lint

- `scripts/format.sh` runs clang-format.
- `scripts/lint.sh` runs clang-tidy (uses compile_commands.json if available).
