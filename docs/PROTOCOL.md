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
- SD.Format (0x0200): formats the SD card (FATFS); response empty. This runs synchronously and will block until mkfs+mount complete.
- SD.List (0x0201): request payload = path\0, response payload = text lines ("D name\n" or "F name size\n"). Paths are relative to the active mount; `/` is the root.
- SD.Read (0x0202): request payload = path\0 <u32 offset><u32 len>, response payload = up to len bytes.
- SD.Write (0x0203): request payload = path\0 <u32 offset><u32 len><data>, response payload = empty on success.
- SD.Rename (0x0204): request payload = old\0 new\0, response empty on success.
- SD.Delete (0x0205): request payload = path\0, response empty on success.
- SD.Mkdir (0x0206): request payload = path\0, response empty on success.
- SD.Stat (0x0207): request payload = path\0, response payload = <u32 size><u32 flags> where bit0=1 if directory.
- SD.Checksum (0x0208): request payload = path\0, response payload = <u32 crc32> over file contents.
- SD.Status (0x0209): no request payload; response payload = <s32 rc> mount status (0 ok, negative errno otherwise).
- SD.Fill (0x020C): request payload = path\0 <u32 size><u32 seed>; device creates/overwrites the file with a deterministic xorshift32 pattern (no serial bulk transfer). Response empty on success.

- Display (0x0300+):
  - Disp.Info (0x0300): no request payload; response payload = <u16 w><u16 h><u8 fmt><u8 can_read><u8 can_write><u16 stride_bytes?>.
  - Disp.Fill (0x0301): request payload = <u8 r><u8 g><u8 b>; response empty on success.
  - Disp.Read (0x0302): request payload = <u16 x><u16 y><u16 w><u16 h>; response payload = raw pixel data in native format.
  - Disp.GetID (0x0303): no request payload; response payload = panel ID bytes (optional; may be empty).
  - Disp.BlankOff (0x0304): unblank/backlight on; response empty.
  - Disp.BlankOn (0x0305): blank/backlight off; response empty.
  - Disp.Diag (0x0306): no request payload; response payload = UTF-8 text diagnostics (ltdc/dsi/panel status and mode).

- Doom (0x0400+):
  - Doom.Start (0x0400): start background task (scaffold); response empty.
  - Doom.Status (0x0401): response payload = <u32 state><u32 ticks> where state: 0=stopped,1=starting,2=running.
  - Doom.Stop (0x0402): stop task; response empty.

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
- Larger payloads can be supported by adjusting internal buffers. UART logs are minimized to avoid interfering with response latencies.
