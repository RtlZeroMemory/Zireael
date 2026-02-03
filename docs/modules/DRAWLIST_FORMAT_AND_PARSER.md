# Module — Drawlist Format and Parser

The drawlist is a versioned, little-endian byte stream consumed by the core to update an in-memory framebuffer.

## Format overview

```
┌──────────────────────────────┐
│ Header (zr_dl_header_t) 64B  │
├──────────────────────────────┤
│ Command stream               │
│   [cmd_header + payload]...  │
├──────────────────────────────┤
│ String spans (zr_dl_span_t[])│
├──────────────────────────────┤
│ String bytes (UTF-8)         │
├──────────────────────────────┤
│ Blob spans (zr_dl_span_t[])  │
├──────────────────────────────┤
│ Blob bytes                   │
└──────────────────────────────┘
```

## On-buffer rules

- Header begins with `zr_dl_header_t` (magic `0x4C44525A` = "ZRDL")
- Offsets and lengths validated before any pointer derivation
- Commands are self-framed: `{opcode, flags, size}` in `zr_dl_cmd_header_t`
- Engine borrows drawlist bytes (no copy); validated views must not outlive the buffer
- 4-byte alignment required for all section boundaries and command sizes

## Versions

Supported drawlist versions are pinned in `include/zr/zr_version.h` and negotiated at `engine_create()` via
`zr_engine_config_t.requested_drawlist_version`.

- **v1 (`ZR_DRAWLIST_VERSION_V1`)**: Stable baseline format and opcode set. MUST remain behavior-stable.
- **v2 (`ZR_DRAWLIST_VERSION_V2`)**: Preserves v1 header layout and framing rules. Adds new opcodes.

Unknown opcodes MUST be rejected with `ZR_ERR_UNSUPPORTED`.

## Opcodes

| Opcode | Value | Version | Payload | Description |
|--------|-------|---------|---------|-------------|
| `ZR_DL_OP_INVALID` | 0 | — | — | Invalid (rejected) |
| `ZR_DL_OP_CLEAR` | 1 | v1 | 0B | Clear framebuffer to default style |
| `ZR_DL_OP_FILL_RECT` | 2 | v1 | 32B | Fill rectangle with style |
| `ZR_DL_OP_DRAW_TEXT` | 3 | v1 | 40B | Draw text from string table |
| `ZR_DL_OP_PUSH_CLIP` | 4 | v1 | 16B | Push clipping rectangle |
| `ZR_DL_OP_POP_CLIP` | 5 | v1 | 0B | Pop clipping rectangle |
| `ZR_DL_OP_DRAW_TEXT_RUN` | 6 | v1 | 16B | Draw pre-measured text run (blob) |
| `ZR_DL_OP_SET_CURSOR` | 7 | v2 | 12B | Set cursor position/shape/visibility |

Command sizes include the 8-byte header (`zr_dl_cmd_header_t`).

## Cursor control (v2)

Drawlist v2 adds `ZR_DL_OP_SET_CURSOR`, which updates the engine-owned *desired cursor state* for subsequent presents.

### Payload (`zr_dl_cmd_set_cursor_t`)

Fixed-width fields (little-endian on-buffer):

- `int32_t x`, `int32_t y` — 0-based cell coordinates. `-1` means "do not change".
- `uint8_t shape` — `0=block`, `1=underline`, `2=bar`
- `uint8_t visible` — `0/1`
- `uint8_t blink` — `0/1`
- Reserved/padding bytes MUST be `0` and are validated.

### Semantics

- This opcode does **not** draw glyphs into the framebuffer.
- The desired cursor state persists until changed by a subsequent `ZR_DL_OP_SET_CURSOR`.
- During `engine_present()`, output emission applies the desired cursor state *after* emitting framebuffer diff bytes.

## Validation contract

Validation happens before any command executes:

- Magic and version match
- All offsets within `total_size`
- No overlapping sections
- Command sizes match opcode expectations exactly
- String/blob indices within bounds
- Reserved fields are zero

On failure: `ZR_ERR_FORMAT` or `ZR_ERR_UNSUPPORTED`, no partial effects.

## Style struct

```c
typedef struct zr_dl_style_t {
  uint32_t fg;        // 0x00RRGGBB
  uint32_t bg;        // 0x00RRGGBB
  uint32_t attrs;     // bold=0, italic=1, underline=2, reverse=3, strikethrough=4
  uint32_t reserved0; // must be 0 in v1
} zr_dl_style_t;
```

## Text runs

`DRAW_TEXT_RUN` references a blob containing multiple styled segments:

```
u32 seg_count
repeat seg_count times:
  zr_dl_style_t style   (16B)
  u32 string_index
  u32 byte_off
  u32 byte_len
```

Total blob length: `4 + seg_count * 28` bytes.

See:

- `src/core/zr_drawlist.h`
- `include/zr/zr_drawlist.h`
