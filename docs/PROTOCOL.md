# Serial Protocol

This project uses a simple framed binary protocol over the UART. The design
emphasizes robustness, resynchronization in the presence of noise/logs, and a
clear separation of layers so that unit tests can mock dependencies.

Frame Format (little-endian)

- Magic: 0xC0 0xDE 0x42 0x17
- Version: 1 byte (currently 1)
- Flags: 1 byte
  - bit0: 1 = response, 0 = request
  - bit1: 1 = has payload
- Command: 2 bytes (u16)
- Length: 4 bytes (u32) payload length
- CRC32: 4 bytes (IEEE 802.3) over header without magic (version..length) and payload. Frames with
  invalid CRC are discarded and the stream resynchronizes.
- Payload: Length bytes

Layering

- protocol: Encoding/decoding and streaming parser (noise-tolerant, can resync).
- cmd_dispatch: Registry and dispatch of command handlers by ID.
- commands: Individual command libraries (echo, version, reboot).
- transport: UART async implementation (platform_serial_*).

Command Semantics

- Echo (0x0001): request payload echoed as response payload.
- Version (0x0002): response payload is ASCII git short hash.
- Reboot (0x0003): device reboots; no response payload.
 - MemRead (0x0100): request payload = <u32 addr><u32 len>, response payload = len bytes read.
 - MemWrite (0x0101): request payload = <u32 addr><u32 len><data>, response payload = empty on success.

Safety

- MemRead allows flash and SRAM reads, limited to 256 bytes per request.
- MemWrite allows writes to SRAM only (flash writes are rejected).

Testing Strategy

- Unit tests:
  - protocol encode/decode with CRC validation
  - cmd_dispatch registration and invocation
  - utility ring buffer behavior (wraparound/overflow)
- Integration tests:
  - Echo roundtrip
  - Version returns expected hash
  - Reboot triggers reboot and subsequent boot message

Notes

- The stream parser tolerates unrelated UART bytes by scanning for magic,
  enforces CRC checks, and resynchronizes on errors (invalid frames are dropped).
- Larger payloads can be supported by adjusting internal buffers.
