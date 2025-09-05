#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
BUILD_DIR="${ROOT_DIR}/build-host/unit"

mkdir -p "${BUILD_DIR}"
cmake -S "${ROOT_DIR}/tests/unit" -B "${BUILD_DIR}" -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
