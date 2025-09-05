# Getting Started

This repo targets Zephyr v3.6.0 and the STM32H747I-DISCO board (M7 core).

- `scripts/setup.sh`: Initializes `.venv`, installs `west`, fetches Zephyr + modules
- `scripts/build.sh`: Builds the Zephyr app for `stm32h747i_disco_m7`
- `scripts/flash.sh`: Flashes over ST-LINK/OpenOCD
- `scripts/monitor.sh`: Opens a serial terminal to the ST-LINK VCP
- `scripts/test_unit.sh`: Builds and runs host unit tests
- `scripts/test_integration.sh`: Runs integration tests (assumes firmware flashed)

Prereqs

- Linux/macOS with Python 3.8+, Git, CMake, Ninja
- Zephyr SDK (preferred) or `arm-none-eabi-gcc` + `dtc`
- OpenOCD in PATH for flashing

Quick Start

```
./scripts/setup.sh
./scripts/build.sh
./scripts/flash.sh
./scripts/monitor.sh  # Expect “[BOOT] …” then periodic “[tick]”
```
