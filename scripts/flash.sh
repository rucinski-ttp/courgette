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
echo "[flash] Flash complete"
