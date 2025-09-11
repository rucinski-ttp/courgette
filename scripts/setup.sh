#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$ROOT_DIR"

ZEPHYR_VERSION="main"

echo "[setup] Creating Python venv at .venv"
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip

echo "[setup] Installing west (into venv)"
pip install --upgrade west

if [ ! -d ".west" ]; then
  echo "[setup] Initializing Zephyr workspace (manifest: ${ZEPHYR_VERSION})"
  west init -m https://github.com/zephyrproject-rtos/zephyr --mr "$ZEPHYR_VERSION" .
else
  echo "[setup] Zephyr workspace already initialized; updating manifest to ${ZEPHYR_VERSION}"
  west config manifest.path zephyr
  west config manifest.url https://github.com/zephyrproject-rtos/zephyr
  west config manifest.revision "$ZEPHYR_VERSION"
fi

echo "[setup] Fetching Zephyr modules (west update)"
west update

echo "[setup] Installing Zephyr Python requirements"
pip install -r zephyr/scripts/requirements.txt

echo "[setup] Installing test requirements"
pip install -r tests/pytest/requirements.txt

echo "[setup] Exporting Zephyr CMake package"
west zephyr-export

echo "[setup] Checking toolchain availability"
if [ -z "${ZEPHYR_SDK_INSTALL_DIR:-}" ] && ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  echo "[setup] ERROR: No Zephyr SDK (ZEPHYR_SDK_INSTALL_DIR) and no arm-none-eabi-gcc found." >&2
  echo "        Install Zephyr SDK or GNU Arm Embedded toolchain and re-run setup." >&2
  exit 1
fi

echo "[setup] Complete. Next: ./scripts/build.sh"
