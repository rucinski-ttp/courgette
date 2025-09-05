#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

FILES=$(git ls-files | grep -E '\.(c|h|cpp|hpp)$' || true)
if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found. Please install it." >&2
  exit 1
fi

if [ -z "${FILES}" ]; then
  echo "No source files to format"
  exit 0
fi

echo "Formatting files..."
echo "${FILES}" | xargs clang-format -i
