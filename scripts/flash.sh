#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$ROOT_DIR"

BOARD="${BOARD:-stm32h747i_disco/stm32h747xx/m7}"
BUILD_DIR="build/${BOARD}"

if [ -d .venv ]; then
  source .venv/bin/activate
fi

west --version >/dev/null || {
  echo "[flash] west not found. Run scripts/setup.sh first." >&2
  exit 1
}

if [ ! -d "$BUILD_DIR" ]; then
  echo "[flash] ERROR: Build directory not found at '${BUILD_DIR}'. Run scripts/build.sh first." >&2
  exit 1
fi

echo "[flash] Flashing ${BOARD}"

# If stm32cubeprogrammer runner is selected by upstream and the CLI is not
# present in PATH, try to add common install locations (user/system).
if ! command -v STM32_Programmer_CLI >/dev/null 2>&1; then
  for D in \
    "/home/${SUDO_USER:-${USER}}/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin" \
    "/home/${SUDO_USER:-${USER}}/.local/share/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin" \
    "/home/*/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin" \
    "/opt/stm32cubeprogrammer/bin" \
    "/opt/st/STM32Cube/STM32CubeProgrammer/bin" \
    "/root/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin"; do
    for P in $D; do
      if [ -d "$P" ] && [ -x "$P/STM32_Programmer_CLI" ]; then
        export PATH="$P:$PATH"
        echo "[flash] Added STM32CubeProgrammer to PATH: $P"
        break 2
      fi
    done
  done
fi

west flash -d "$BUILD_DIR" --skip-rebuild
