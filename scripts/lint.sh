#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

if ! command -v clang-tidy >/dev/null 2>&1; then
  echo "clang-tidy not found. Please install it." >&2
  exit 1
fi

FILES=$(git ls-files | grep -E '^lib/' | grep -E '\.(c|cpp)$' || true)
if [ -z "${FILES}" ]; then
  echo "No files to lint"
  exit 0
fi

echo "Running clang-tidy on:"
echo "${FILES}" | tr '\n' ' ' && echo

# Use compile_commands.json if present (from Zephyr build or unit build)
DB="${ROOT_DIR}/build-host/unit/compile_commands.json"

EXTRA=()
if [ -f "$DB" ]; then
  EXTRA=("-p" "$(dirname "$DB")")
else
  echo "Warning: compile_commands.json not found; clang-tidy may miss includes." >&2
fi

clang-tidy ${EXTRA[@]:-} ${FILES}
