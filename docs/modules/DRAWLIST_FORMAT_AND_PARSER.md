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
- **v3 (`ZR_DRAWLIST_VERSION_V3`)**: Preserves v1/v2 framing and opcode set. Extends style payloads with underline color
  and hyperlink string-table references.
- **v4 (`ZR_DRAWLIST_VERSION_V4`)**: Preserves v1/v2/v3 framing rules. Adds `DRAW_CANVAS` for RGBA sub-cell blitting.
- **v5 (`ZR_DRAWLIST_VERSION_V5`)**: Preserves v1-v4 framing rules. Adds `DRAW_IMAGE` for terminal image protocols with
  deterministic fallback.

Hyperlink architecture decision:

- No hyperlink opcode was added.
- Hyperlinks flow through existing draw ops by carrying URI/ID references in v3 style payloads, resolved to framebuffer
  `link_ref` values during execution.

Unknown opcodes MUST be rejected with `ZR_ERR_UNSUPPORTED`.

## Opcodes

| Opcode | Value | Version | Payload | Description |
|--------|-------|---------|---------|-------------|
| `ZR_DL_OP_INVALID` | 0 | — | — | Invalid (rejected) |
| `ZR_DL_OP_CLEAR` | 1 | v1+ | 0B | Clear framebuffer to default style |
| `ZR_DL_OP_FILL_RECT` | 2 | v1+ | 32B | Fill rectangle with style |
| `ZR_DL_OP_DRAW_TEXT` | 3 | v1+ | 40B | Draw text from string table |
| `ZR_DL_OP_PUSH_CLIP` | 4 | v1+ | 16B | Push clipping rectangle |
| `ZR_DL_OP_POP_CLIP` | 5 | v1+ | 0B | Pop clipping rectangle |
| `ZR_DL_OP_DRAW_TEXT_RUN` | 6 | v1+ | 16B | Draw pre-measured text run (blob) |
| `ZR_DL_OP_SET_CURSOR` | 7 | v2+ | 12B | Set cursor position/shape/visibility |
| `ZR_DL_OP_DRAW_CANVAS` | 8 | v4+ | 24B | Blit RGBA canvas bytes into framebuffer cells |
| `ZR_DL_OP_DRAW_IMAGE` | 9 | v5+ | 32B | Emit protocol image command or fallback sub-cell blit |

Command sizes include the 8-byte header (`zr_dl_cmd_header_t`).

Version-specific command sizes:

- `CLEAR`: 8B in all versions
- `FILL_RECT`: 40B in v1/v2, 52B in v3
- `DRAW_TEXT`: 48B in v1/v2, 60B in v3
- `DRAW_TEXT_RUN`: 24B in all versions (segment payload inside blob changes in v3)
- `SET_CURSOR`: 20B in v2/v3/v4
- `DRAW_CANVAS`: 32B in v4
- `DRAW_IMAGE`: 40B in v5

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

## Canvas blit (v4)

Drawlist v4 adds `ZR_DL_OP_DRAW_CANVAS` (`zr_dl_cmd_draw_canvas_t`), which renders RGBA bytes through the sub-cell
blitter pipeline into framebuffer cells.

Payload fields:

- `dst_col`, `dst_row`, `dst_cols`, `dst_rows` — destination rectangle in cells
- `px_width`, `px_height` — source rectangle in pixels
- `blob_offset`, `blob_len` — byte range inside blob-bytes section
- `blitter` — `zr_blitter_t` selector (`AUTO`, `BRAILLE`, `SEXTANT`, etc.)
- `flags`, `reserved` — must be `0`

Validation rules:

- zero dimensions are rejected
- `blob_len` must equal `px_width * px_height * 4` (checked arithmetic)
- `blob_offset + blob_len` must be in-bounds of `blobs_bytes`
- unknown `blitter` values are rejected
- command is `ZR_ERR_UNSUPPORTED` on drawlist versions `< v4`

Execution notes:

- nearest-neighbor integer sampling maps source pixels to destination sub-cells
- transparent-only cells are skipped (existing framebuffer content preserved)
- writes use framebuffer painter path, so clip stack behavior matches `FILL_RECT`/`DRAW_TEXT`
- malformed command payloads fail validation with `ZR_ERR_FORMAT`; execute-time bounds failures fail with `ZR_ERR_INVALID_ARGUMENT`

## Protocol image (v5)

Drawlist v5 adds `ZR_DL_OP_DRAW_IMAGE` (`zr_dl_cmd_draw_image_t`) for protocol-backed image rendering.

Payload fields:

- `dst_col`, `dst_row`, `dst_cols`, `dst_rows` — destination rectangle in cells
- `px_width`, `px_height` — source image dimensions in pixels
- `blob_offset`, `blob_len` — payload byte range inside blob-bytes section
- `image_id` — stable wrapper-provided image key for cache reuse
- `format` — `RGBA` or `PNG`
- `protocol` — `auto`, `kitty`, `sixel`, `iterm2`
- `z_layer` — `-1`, `0`, `1`
- `fit_mode` — `fill`, `contain`, `cover`
- `flags`, `reserved0`, `reserved1` — must be zero

Validation rules:

- non-zero destination/source dimensions
- valid enum ranges for protocol/format/fit mode
- `z_layer` in `[-1, 1]`
- `blob_offset + blob_len` in-bounds of `blobs_bytes`
- for `RGBA`, `blob_len == px_width * px_height * 4`
- for `PNG`, `blob_len > 0`

Execution rules:

- protocol selected from explicit request or terminal profile auto-detection
- if protocol is unavailable:
  - `RGBA` falls back to sub-cell `ZR_BLIT_AUTO`
  - `PNG` returns `ZR_ERR_UNSUPPORTED`
- when protocol is available, command/payload are copied into engine-owned image staging and emitted during present
- kitty/sixel require `RGBA`; iTerm2 accepts `RGBA` and `PNG`

## Validation contract

Validation happens before any command executes:

- Magic and version match
- All offsets within `total_size`
- No overlapping sections
- Command sizes match opcode expectations exactly
- String/blob indices within bounds
- `DRAW_TEXT_RUN` blobs are validated in deterministic phases:
  span resolution -> framing-size check -> per-segment string-slice bounds
- `DRAW_IMAGE` checks protocol enums, payload sizing, and fallback constraints before execution
- Reserved fields are zero

On failure: `ZR_ERR_FORMAT` or `ZR_ERR_UNSUPPORTED`, no partial effects.

## Style struct

```c
typedef struct zr_dl_style_t {
  uint32_t fg;        // 0x00RRGGBB
  uint32_t bg;        // 0x00RRGGBB
  uint32_t attrs;     // bold=0, italic=1, underline=2, reverse=3, dim=4, strikethrough=5, overline=6, blink=7
  uint32_t reserved0; // must be 0 in v1
} zr_dl_style_t;
```

v3 adds a style extension payload after `zr_dl_style_t`:

```c
typedef struct zr_dl_style_v3_ext_t {
  uint32_t underline_rgb; // 0x00RRGGBB; 0 = terminal default underline color
  uint32_t link_uri_ref;  // 1-based string-table ref; 0 = no hyperlink
  uint32_t link_id_ref;   // optional 1-based string-table ref; 0 = no id param
} zr_dl_style_v3_ext_t;
```

Validation rules for v3 extension:

- `link_uri_ref == 0` means no link; `link_id_ref` may be zero/non-zero but must be in range if non-zero.
- Non-zero refs must point to valid string spans.
- URI span length must be `1..ZR_FB_LINK_URI_MAX_BYTES`.
- ID span length must be `0..ZR_FB_LINK_ID_MAX_BYTES`.
- During execute, URI/ID bytes are interned into framebuffer link table and converted to `style.link_ref`.

## Text runs

`DRAW_TEXT_RUN` references a blob containing multiple styled segments:

```
u32 seg_count
repeat seg_count times:
  style payload         (16B in v1/v2, 28B in v3)
  u32 string_index
  u32 byte_off
  u32 byte_len
```

Total blob length:

- v1/v2: `4 + seg_count * 28`
- v3: `4 + seg_count * 40`

See:

- `src/core/zr_drawlist.h`
- `include/zr/zr_drawlist.h`
