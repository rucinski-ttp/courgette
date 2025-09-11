#!/usr/bin/env bash
set -euo pipefail

# Format code and run clang-tidy. Defaults to host (unit) targets only.
# Usage:
#   ./scripts/tidy.sh [--no-format] [--no-host] [--firmware] [--board <BOARD>]

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$ROOT_DIR"

DO_FORMAT=1
DO_HOST=1
DO_FW=0
# Match build.sh default BOARD layout
BOARD="${BOARD:-stm32h747i_disco/stm32h747xx/m7}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-format) DO_FORMAT=0; shift;;
    --no-host) DO_HOST=0; shift;;
    --firmware) DO_FW=1; shift;;
    --board) BOARD="$2"; shift 2;;
    *) echo "Unknown arg: $1" >&2; exit 2;;
  esac
done

have() { command -v "$1" >/dev/null 2>&1; }

if [[ $DO_FORMAT -eq 1 ]]; then
  if ! have clang-format; then
    echo "[tidy] clang-format not found; please install (e.g., apt install clang-format)" >&2
    exit 1
  fi
  echo "[tidy] Running clang-format (in-place) on tracked C/C++ files..."
  mapfile -t FILES < <(git ls-files | grep -E '\\.(c|h|cc|cpp|hpp)$' || true)
  if [[ ${#FILES[@]} -gt 0 ]]; then
    clang-format -i "${FILES[@]}"
  fi
fi

if [[ $DO_HOST -eq 1 ]]; then
  if ! have run-clang-tidy && ! have clang-tidy; then
    echo "[tidy] clang-tidy not found; please install (e.g., apt install clang-tidy)" >&2
    exit 1
  fi
  echo "[tidy] Ensuring host unit build exists (for compile_commands.json)..."
  ./scripts/test_unit.sh >/dev/null
  echo "[tidy] Running clang-tidy on host libraries (per compile DB, limited to lib/*)..."
  # Limit to lib/* sources to avoid noise from tests and third-party code in the host build
  if have jq; then
    mapfile -t HOSTFILES < <(jq -r '.[] | select(.file|test("/(lib)/")) | .file' build-host/unit/compile_commands.json 2>/dev/null | sort -u)
  else
    mapfile -t HOSTFILES < <(grep -o '"file": ".*"' build-host/unit/compile_commands.json 2>/dev/null | sed -E 's/"file": "(.*)"/\1/' | grep -E '/lib/' | sort -u)
  fi
  if [[ ${#HOSTFILES[@]} -eq 0 ]]; then
    echo "[tidy] No lib/* files found in host compile DB; skipping host tidy."
  else
    if have run-clang-tidy; then
      run-clang-tidy -quiet -p build-host/unit -header-filter='^(lib)/.*' -- "${HOSTFILES[@]}" || true
    else
      clang-tidy -p build-host/unit -quiet -header-filter='^(lib)/.*' "${HOSTFILES[@]}" || true
    fi
  fi
fi

if [[ $DO_FW -eq 1 ]]; then
  if ! have run-clang-tidy && ! have clang-tidy; then
    echo "[tidy] clang-tidy not found; skipping firmware tidy" >&2
  else
    echo "[tidy] Ensuring firmware build exists for ${BOARD}..."
    BOARD="$BOARD" ./scripts/build.sh >/dev/null
    echo "[tidy] Attempting clang-tidy on firmware sources (best-effort; Zephyr GCC flags may cause noise)..."
    # Best-effort: run on app/ and lib/ entries present in the Zephyr compile DB
    DB="build/${BOARD}/compile_commands.json"
    if [[ -f "$DB" ]]; then
      if have jq; then
        mapfile -t FWFILES < <(jq -r '.[] | select(.file|test("/(app|lib)/")) | .file' "$DB" | sort -u)
      else
        mapfile -t FWFILES < <(grep -o '"file": ".*"' "$DB" | sed -E 's/"file": "(.*)"/\1/' | grep -E '/(app|lib)/' | sort -u)
      fi
      if [[ ${#FWFILES[@]} -gt 0 ]]; then
        if have run-clang-tidy; then
          run-clang-tidy -quiet -p "build/${BOARD}" -header-filter='^(app|lib)/.*' "${FWFILES[@]}" || true
        else
          clang-tidy -p "build/${BOARD}" -quiet -header-filter='^(app|lib)/.*' "${FWFILES[@]}" || true
        fi
      fi
    fi
  fi
fi

echo "[tidy] Done."
