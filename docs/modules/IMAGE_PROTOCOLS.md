# Module — Image Protocols

This module defines deterministic terminal image emission for drawlist v1 `DRAW_IMAGE` commands.

## Scope

- Protocols: Kitty Graphics, Sixel, iTerm2 inline images.
- Selection: explicit request or deterministic auto-selection from terminal profile flags.
- Fallback: when no protocol is available, RGBA payloads render through sub-cell blitters (`ZR_BLIT_AUTO`).
- Ownership: image command/blob bytes are copied into engine-owned staging memory during drawlist execution.

## Protocol Selection

`zr_image_select_protocol(requested_protocol, profile)` uses this policy:

1. Explicit request wins (`kitty`, `sixel`, `iterm2`).
2. `requested_protocol == 0` (auto):
   - kitty if `profile->supports_kitty_graphics`
   - else sixel if `profile->supports_sixel`
   - else iTerm2 if `profile->supports_iterm2_images`
   - else none
3. Unknown explicit value returns none.

Determinism rule: identical `(requested_protocol, profile flags)` must produce the same selected protocol.

## Drawlist v1 `DRAW_IMAGE`

`ZR_DL_OP_DRAW_IMAGE` payload (`zr_dl_cmd_draw_image_t`) carries:

- destination rectangle in cells (`dst_col`, `dst_row`, `dst_cols`, `dst_rows`)
- source geometry (`px_width`, `px_height`)
- blob range (`blob_offset`, `blob_len`)
- stable image key (`image_id`)
- format (`RGBA` or `PNG`)
- protocol request (`auto`, `kitty`, `sixel`, `iterm2`)
- fit mode (`fill`, `contain`, `cover`)
- z-layer (`-1`, `0`, `1`)

Validation rules:

- all dimensions must be non-zero
- reserved fields must be zero
- protocol/format/fit enums must be valid
- `blob_offset + blob_len` must be in-bounds
- for RGBA payloads, `blob_len == px_width * px_height * 4`

Execution rules:

- protocol selected as above and frozen into staging (present emits the resolved protocol, not the raw request)
- if protocol is none:
  - RGBA -> immediate fallback blit into framebuffer
  - PNG -> `ZR_ERR_UNSUPPORTED`
- if protocol is available:
  - command is copied into `zr_image_frame_t` staging
  - bytes are emitted in present path from staged image frame

## Kitty Protocol

Emitter API:

- `zr_image_kitty_emit_transmit_rgba`
- `zr_image_kitty_emit_place`
- `zr_image_kitty_emit_delete`

Encoding details:

- uses APC framing: `ESC _ G ... ESC \\`
- RGBA payload is RFC4648 base64 (`f=32`)
- transmit chunks are bounded (raw chunk <= 3072 bytes, base64 chunk <= 4096 bytes)
- placement uses CUP + APC (`a=p`)
- deletion uses APC (`a=d,d=i`)

## Kitty Cache and Lifecycle

`zr_image_state_t` keeps a bounded LRU cache (`ZR_IMAGE_CACHE_SIZE = 64`):

- cache hit by `(image_id, hash, dimensions)` or `(hash, dimensions)` reuses transmitted kitty image id
- cache miss transmits a new kitty image id
- full cache evicts least-recently-used transmitted slot (delete before reuse)
- frame begin clears `placed_this_frame`
- frame end deletes transmitted slots not placed in the current frame

Determinism rule: for the same frame sequence and command payloads, kitty transmit/place/delete byte order is stable.

## Sixel Protocol

Emitter API: `zr_image_sixel_emit_rgba`.

Pipeline:

1. RGBA quantization to 6x6x6 keyspace (`216` colors).
2. Alpha thresholding (`alpha < 128` -> transparent).
3. Palette emission as `#idx;2;R;G;B` percentages.
4. Band rendering in 6-row bands with deterministic run-length encoding (`!n<char>` when run >= 4).
5. DCS framing with final `ESC \\`.

## iTerm2 Protocol

Emitter APIs:

- `zr_image_iterm2_emit_png`
- `zr_image_iterm2_emit_rgba`

Pipeline:

- RGBA path encodes deterministic PNG in-engine (IHDR + stored-deflate IDAT + IEND).
- payload bytes are base64 encoded.
- output uses OSC 1337 `File=inline=1` with width/height/size metadata and BEL terminator.

## Fit Modes

`zr_image_scale_rgba` supports deterministic nearest-neighbor modes:

- `FILL`: stretch to target pixels
- `CONTAIN`: letterbox/pillarbox with transparent fill
- `COVER`: center crop after scaling to cover target

Target pixel size derives from destination cell rectangle and profile cell metrics (default 8x16 when unknown).

## Present-Path Emission

Image sideband emission runs in `engine_present()` after framebuffer diff generation and before final platform write.

- no per-image heap allocation in hot path: uses arena + bounded builders
- single-flush policy remains intact (`plat_write_output()` once on success)
- failure returns `ZR_ERR_*` with no partial platform writes

## Test Expectations

Required deterministic coverage:

- base64 vectors and limits
- selector precedence
- kitty/sixel/iTerm2 byte outputs
- fit mode outputs
- cache/lifecycle transitions
- drawlist v1 `DRAW_IMAGE` validate/execute paths
- golden fixtures under `tests/golden/fixtures/image_*`
