# Module — Sub-Cell Blitter Infrastructure

This module defines deterministic conversion of RGBA pixel buffers into framebuffer cells using Unicode sub-cell glyphs.

## Scope

- Input: `zr_blit_input_t` (RGBA8 bytes, width/height, row stride)
- Output: `zr_cell_t` writes through `zr_fb_put_grapheme()`
- No platform I/O, no OS headers, no per-frame heap allocations

## Blitter Modes

`zr_blitter_t` values:

- `ZR_BLIT_AUTO`
- `ZR_BLIT_PIXEL` (reserved)
- `ZR_BLIT_BRAILLE` (2x4)
- `ZR_BLIT_SEXTANT` (2x3)
- `ZR_BLIT_QUADRANT` (2x2)
- `ZR_BLIT_HALFBLOCK` (1x2)
- `ZR_BLIT_ASCII` (1x1)

## AUTO Selection

AUTO selection uses `zr_blit_caps_t`:

1. If dumb/pipe mode: `ASCII`
2. If Unicode unavailable: `ASCII`
3. Optional `include_braille_in_auto`: `BRAILLE`
4. If sextant-capable: `SEXTANT`
5. Else if quadrant-capable: `QUADRANT`
6. Else if halfblock-capable: `HALFBLOCK`
7. Else: `ASCII`

Explicit non-AUTO requests are honored (except reserved `ZR_BLIT_PIXEL`, which returns `ZR_ERR_UNSUPPORTED`).

## Sampling (Nearest Neighbor, Integer)

For destination sub-pixel coordinate `(sub_x, sub_y)`:

- `src_x = (sub_x * px_width) / (dst_cols * sub_w)`
- `src_y = (sub_y * px_height) / (dst_rows * sub_h)`

All math is integer and deterministic.

## Color + Threshold Formulas

Squared RGB distance:

`dist = (dr*dr) + (dg*dg) + (db*db)`

BT.709 integer luminance:

`luma = (r*2126 + g*7152 + b*722) / 10000`

## Alpha Handling

- `alpha < 128`: transparent sample
- `alpha >= 128`: opaque sample

Transparent-only cells are skipped (existing framebuffer content remains).

## Glyph Mappings

### Halfblock (1x2)

- `U+2580` upper half
- `U+2584` lower half
- `U+2588` full block
- `U+0020` space

### Quadrant (2x2)

16-entry mapping by bitmask (bit0=TL, bit1=TR, bit2=BL, bit3=BR) to Block Elements glyphs.

### Sextant (2x3)

64-entry mapping by bitmask (bit0..bit5 as [TL,TR,ML,MR,BL,BR]).

Fallback masks (missing sextant codepoints):

- `0x00 -> U+0020`
- `0x15 -> U+258C`
- `0x2A -> U+2590`
- `0x3F -> U+2588`

### Braille (2x4)

`codepoint = 0x2800 + pattern`

Bit layout by row/column:

- row0: left bit0, right bit3
- row1: left bit1, right bit4
- row2: left bit2, right bit5
- row3: left bit6, right bit7

## Determinism Guarantees

- Integer-only hot path
- Stable tie-breakers (lower mask on equal error)
- No locale/time/random dependencies
- Same input bytes + mode + caps => same cell output
