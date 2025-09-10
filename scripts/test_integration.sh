#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

# Build and flash to ensure a known-good image before running tests
"$(dirname "$0")"/build.sh
"$(dirname "$0")"/flash.sh

# Expect environment to be prepared (pytest/pyserial installed)
command -v pytest >/dev/null 2>&1 || {
  echo "[itest] ERROR: pytest not found. Run scripts/setup.sh or install requirements." >&2
  exit 1
}

pytest tests/pytest -q "$@"
