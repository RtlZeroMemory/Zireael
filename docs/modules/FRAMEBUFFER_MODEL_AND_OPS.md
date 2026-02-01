# Module — Framebuffer Model and Ops

This module documents the in-memory framebuffer used by drawlist execution and internal renderers.

Defined by `src/core/zr_fb.h` / `src/core/zr_fb.c`.

## Data model

### Cells

Each cell (`zr_fb_cell_t`) stores:

- `glyph[ZR_FB_GLYPH_MAX_BYTES]` — UTF-8 bytes for a **single grapheme cluster**
- `glyph_len` — number of valid bytes in `glyph`
- `style` — foreground/background/attributes (`zr_style_t`)
- `flags` — includes `ZR_FB_CELL_FLAG_CONTINUATION`

### Grapheme size cap

If a grapheme cluster’s UTF-8 encoding exceeds `ZR_FB_GLYPH_MAX_BYTES`, the framebuffer replaces it with `U+FFFD`
deterministically.

## Wide glyph continuation invariant

Some graphemes consume 2 terminal columns (best-effort width policy; deterministic pins in `src/unicode/`).

When the framebuffer stores a wide glyph at `(x, y)`:

- `(x, y)` is the **lead cell**:
  - `flags` does NOT include `ZR_FB_CELL_FLAG_CONTINUATION`
  - `glyph_len > 0` and `glyph[]` contains the grapheme UTF-8 bytes
- `(x+1, y)` is the **continuation cell**:
  - `flags` includes `ZR_FB_CELL_FLAG_CONTINUATION`
  - `glyph_len == 0`

Writers that overwrite cells MUST preserve these invariants. In particular, internal overlays/diff helpers MUST NOT
create a state where a continuation cell exists without a valid lead cell immediately to its left.

## Ops overview

- `zr_fb_init` — binds caller-owned backing store; never allocates.
- `zr_fb_clear` / `zr_fb_fill_rect` — style-based fills.
- `zr_fb_draw_text_bytes` — draws UTF-8 text, storing graphemes into cells and setting continuation flags for width-2
  glyphs.

