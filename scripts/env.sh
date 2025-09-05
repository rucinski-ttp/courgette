#!/usr/bin/env bash
# Source project environment: Python venv and helpful defaults.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$ROOT_DIR"

if [ -d .venv ]; then
  # shellcheck disable=SC1091
  source .venv/bin/activate
else
  echo "[env] WARNING: .venv not found. Run scripts/setup.sh first." >&2
fi

export BOARD="${BOARD:-stm32h747i_disco_m7}"
echo "[env] Activated. BOARD=${BOARD}"

