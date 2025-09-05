#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

# Expect environment to be prepared (pytest/pyserial installed, board flashed)
command -v pytest >/dev/null 2>&1 || {
  echo "[itest] ERROR: pytest not found. Run scripts/setup.sh or install requirements." >&2
  exit 1
}

pytest tests/pytest -q "$@"
