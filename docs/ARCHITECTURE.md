# Architecture

Top-level layout:

- `app/`: Zephyr-facing application and platform-specific code
  - `src/app/`: main entry and application orchestration
  - `src/drivers/`: board/SoC-specific drivers (e.g., heartbeat LED)
  - `src/platform/`: platform abstractions (UART, storage, IPC)
- `lib/`: portable libraries (unit-testable on host)
  - `util/`: utility helpers (logging backends, ring buffers, etc.)
  - `protocol/`: serial/transport protocol layer (scaffolded)
- `external/`: third-party dependencies (e.g., doom port submodule)
- `tests/`: host unit tests and Python integration tests

Build Targets

- Each subfolder builds as a static library target and is linked into the app
  binary. Host tests re-use libraries from `lib/`.

Planned Components

- Async UART: non-blocking TX/RX with ring buffers and ISR-driven queues
- Message protocol: framed, checksummed, resumable transfers for robust SD card
  operations over serial
- Python tooling: SD formatting/listing/reading/writing over the protocol
- Game port: integrate a Doom port as a separate module

