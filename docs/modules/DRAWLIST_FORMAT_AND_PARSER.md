# Module — Drawlist Format and Parser

The drawlist is a versioned, little-endian byte stream consumed by the core to update an in-memory framebuffer.

## On-buffer rules

- Header begins with `zr_dl_header_t`.
- Offsets/lengths are validated before any pointer derivation.
- Commands are self-framed with `{opcode, flags, size}` (`zr_dl_cmd_header_t`).
- The engine borrows the drawlist bytes (no copying); validated views must not outlive the underlying buffer.

See:

- `src/core/zr_drawlist.h`

