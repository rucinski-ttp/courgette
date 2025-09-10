#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

if ! command -v clang-tidy >/dev/null 2>&1; then
  echo "clang-tidy not found. Please install it." >&2
  exit 1
fi

APP_FILES=$(git ls-files | grep -E '^app/' | grep -E '\.(c|cpp)$' || true)
LIB_FILES=$(git ls-files | grep -E '^lib/' | grep -E '\.(c|cpp)$' || true)

if [ -z "${APP_FILES}${LIB_FILES}" ]; then
  echo "No files to lint"
  exit 0
fi

ZE_DB="${ROOT_DIR}/build/stm32h747i_disco_m7/compile_commands.json"
UNIT_DB="${ROOT_DIR}/build-host/unit/compile_commands.json"

if [ -n "$APP_FILES" ]; then
  if [ ! -f "$ZE_DB" ]; then
    echo "[lint] ERROR: Zephyr compile_commands.json not found at $ZE_DB. Run scripts/build.sh first." >&2
    exit 1
  fi
  echo "[lint] app files:"; echo "$APP_FILES" | tr '\n' ' ' && echo
  clang-tidy -p "$(dirname "$ZE_DB")" ${APP_FILES}
fi

if [ -n "$LIB_FILES" ]; then
  if [ ! -f "$UNIT_DB" ]; then
    echo "[lint] ERROR: unit compile_commands.json not found at $UNIT_DB. Run scripts/test_unit.sh to generate it." >&2
    exit 1
  fi
  echo "[lint] lib files:"; echo "$LIB_FILES" | tr '\n' ' ' && echo
  clang-tidy -p "$(dirname "$UNIT_DB")" ${LIB_FILES}
fi
