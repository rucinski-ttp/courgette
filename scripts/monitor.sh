#!/usr/bin/env bash
set -euo pipefail

# Simple serial monitor for ST-LINK VCP.
# Usage: ./scripts/monitor.sh [PORT] [BAUD]

PORT="${1:-${ZEPHYR_SERIAL:-}}"
BAUD="${2:-115200}"

detect_port() {
  if [ -n "$PORT" ]; then
    echo "$PORT"
    return
  fi

  # Prefer by-id symlinks when present (Linux)
  if [ -d /dev/serial/by-id ]; then
    CANDIDATE=$(ls -1 /dev/serial/by-id 2>/dev/null | grep -i -E 'st.*link|stmicro|stm32' | head -n1 || true)
    if [ -n "$CANDIDATE" ]; then
      readlink -f "/dev/serial/by-id/$CANDIDATE"
      return
    fi
  fi

  # Common device patterns
  if compgen -G "/dev/ttyACM*" > /dev/null; then
    ls -1 /dev/ttyACM* | head -n1
    return
  fi
  if compgen -G "/dev/ttyUSB*" > /dev/null; then
    ls -1 /dev/ttyUSB* | head -n1
    return
  fi
  if compgen -G "/dev/cu.usbmodem*" > /dev/null; then
    ls -1 /dev/cu.usbmodem* | head -n1
    return
  fi
  if compgen -G "/dev/cu.usbserial*" > /dev/null; then
    ls -1 /dev/cu.usbserial* | head -n1
    return
  fi

  echo ""  # not found
}

if [ -d .venv ]; then
  source .venv/bin/activate
fi

python - <<'PY'
import sys
try:
    import serial  # noqa: F401
except Exception:
    print('[monitor] ERROR: pyserial not installed. Run:')
    print('  source .venv/bin/activate && pip install -r tests/pytest/requirements.txt')
    sys.exit(1)
PY

PORT=$(detect_port)
if [ -z "$PORT" ]; then
  echo "[monitor] ERROR: Could not auto-detect serial port. Specify explicitly:"
  echo "  ./scripts/monitor.sh /dev/ttyACM0 115200"
  exit 1
fi

echo "[monitor] Connecting to ${PORT} @ ${BAUD}"
python -m serial.tools.miniterm "$PORT" "$BAUD" --raw --eol LF
