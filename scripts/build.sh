#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$ROOT_DIR"

BOARD="${BOARD:-stm32h747i_disco_m7}"
BUILD_DIR="build/${BOARD}"
APP_DIR="app"

if [ -d .venv ]; then
  source .venv/bin/activate
fi

echo "[build] Board: ${BOARD}"
echo "[build] Build dir: ${BUILD_DIR}"

# Toolchain detection: prefer Zephyr SDK if present; otherwise try GNU Arm Embedded.
EXTRA_CMAKE_ARGS=()
if [ -n "${ZEPHYR_SDK_INSTALL_DIR:-}" ]; then
  echo "[build] Using Zephyr SDK at ${ZEPHYR_SDK_INSTALL_DIR}"
  export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
  export ZEPHYR_SDK_INSTALL_DIR
else
  if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    ARM_GCC_BIN=$(dirname "$(command -v arm-none-eabi-gcc)")
    ARM_GCC_ROOT=$(cd "$ARM_GCC_BIN/.." && pwd)
    echo "[build] Using GNU Arm Embedded toolchain at ${ARM_GCC_ROOT}"
    EXTRA_CMAKE_ARGS+=("-DZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb" "-DGNUARMEMB_TOOLCHAIN_PATH=${ARM_GCC_ROOT}")
  else
    echo "[build] ERROR: No toolchain detected. Set ZEPHYR_SDK_INSTALL_DIR or install arm-none-eabi-gcc." >&2
    exit 1
  fi
fi

west --version >/dev/null || {
  echo "[build] west not found. Run scripts/setup.sh first." >&2
  exit 1
}

mkdir -p "$BUILD_DIR"
west build -p auto -b "$BOARD" -d "$BUILD_DIR" "$APP_DIR" -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ${EXTRA_CMAKE_ARGS[@]:-}

echo "[build] Build completed: ${BUILD_DIR}"
