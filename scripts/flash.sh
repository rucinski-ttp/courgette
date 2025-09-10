#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$ROOT_DIR"

BOARD="${BOARD:-stm32h747i_disco_m7}"
BUILD_DIR="build/${BOARD}"

if [ -d .venv ]; then
  source .venv/bin/activate
fi

west --version >/dev/null || {
  echo "[flash] west not found. Run scripts/setup.sh first." >&2
  exit 1
}

command -v openocd >/dev/null 2>&1 || {
  echo "[flash] ERROR: openocd not found in PATH." >&2
  exit 1
}

if [ ! -d "$BUILD_DIR" ]; then
  echo "[flash] ERROR: Build directory not found at '${BUILD_DIR}'. Run scripts/build.sh first." >&2
  exit 1
fi

echo "[flash] Flashing ${BOARD} using west flash"
west flash -d "$BUILD_DIR" --skip-rebuild
echo "[flash] Flash complete; issuing reset+run via OpenOCD"
OCD_CFG=$(find "$(pwd)/zephyr/boards" -type f -name "openocd_${BOARD}.cfg" -print -quit)
if [ -n "$OCD_CFG" ]; then
  openocd -f "$OCD_CFG" -c "init; reset run; sleep 2000; shutdown" >/dev/null 2>&1 || true
  echo "[flash] Target resumed"
else
  echo "[flash] WARN: Could not find OpenOCD cfg for ${BOARD}; skipping reset-run"
fi
