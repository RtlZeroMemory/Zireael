# Module — Drawlist Format and Parser

The drawlist is a versioned, little-endian byte stream consumed by the core to update an in-memory framebuffer.

## On-buffer rules

- Header begins with `zr_dl_header_t`.
- Offsets/lengths are validated before any pointer derivation.
- Commands are self-framed with `{opcode, flags, size}` (`zr_dl_cmd_header_t`).
- The engine borrows the drawlist bytes (no copying); validated views must not outlive the underlying buffer.

See:

- `src/core/zr_drawlist.h`

## Versions

Supported drawlist versions are pinned in `include/zr/zr_version.h` and negotiated at `engine_create()` via
`zr_engine_config_t.requested_drawlist_version`.

- **v1 (`ZR_DRAWLIST_VERSION_V1`)**:
  - Stable baseline format and opcode set.
  - MUST remain behavior-stable.
- **v2 (`ZR_DRAWLIST_VERSION_V2`)**:
  - Preserves v1 header layout and framing rules.
  - Adds new opcodes while keeping v1 opcodes supported.

Unknown opcodes MUST be rejected with `ZR_ERR_UNSUPPORTED`.

## Cursor control (v2)

Drawlist v2 adds `ZR_DL_OP_SET_CURSOR`, which updates the engine-owned *desired cursor state* for subsequent presents.

### Payload (`zr_dl_cmd_set_cursor_t`)

Fixed-width fields (little-endian on-buffer):

- `int32_t x`, `int32_t y` — 0-based cell coordinates.
  - `-1` is allowed and means “do not change that coordinate”.
- `uint8_t shape` — `0=block`, `1=underline`, `2=bar`.
- `uint8_t visible` — `0/1`.
- `uint8_t blink` — `0/1`.
- Reserved/padding bytes MUST be `0` and are validated.

### Semantics

- This opcode does **not** draw glyphs into the framebuffer.
- The desired cursor state persists until changed by a subsequent `ZR_DL_OP_SET_CURSOR`.
- During `engine_present()`, output emission applies the desired cursor state *after* emitting framebuffer diff bytes,
  so diff rendering and cursor control do not fight.
