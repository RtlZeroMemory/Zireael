# Module — Framebuffer Model and Ops

This module documents the in-memory framebuffer used by drawlist execution and internal renderers.

Defined by `src/core/zr_framebuffer.h` / `src/core/zr_framebuffer.c`.

Compatibility shim: `src/core/zr_fb.h` includes `core/zr_framebuffer.h`.

## Data model

### Shared structs

- `zr_rect_t { int32_t x, y, w, h; }`
- `zr_style_t { uint32_t fg_rgb, bg_rgb, attrs, reserved; }`
  - `reserved` is a v1 field and MUST be `0`.

### Cells

Each cell (`zr_cell_t`) stores:

- `glyph[ZR_CELL_GLYPH_MAX]` — UTF-8 bytes for a **single grapheme**
- `glyph_len` — number of valid bytes in `glyph`
- `style` — foreground/background/attributes (`zr_style_t`)
- `width` — one of:
  - `1` for a normal cell
  - `2` for a wide lead cell (spans this cell + the next cell)
  - `0` for a wide continuation cell (must be empty)

### Grapheme size cap

If a grapheme’s UTF-8 encoding exceeds `ZR_CELL_GLYPH_MAX`, the framebuffer replaces it with `U+FFFD`
deterministically (UTF-8 bytes `EF BF BD`, width `1`).

## Wide glyph continuation invariant

Some graphemes consume 2 terminal columns (best-effort width policy; deterministic pins in `src/unicode/`).

When the framebuffer stores a wide glyph at `(x, y)`:

- `(x, y)` is the **lead cell**:
  - `width == 2`
  - `glyph_len > 0` and `glyph[]` contains the grapheme UTF-8 bytes
- `(x+1, y)` is the **continuation cell**:
  - `width == 0`
  - `glyph_len == 0`

Writers that overwrite cells MUST preserve these invariants. In particular, internal overlays/diff helpers MUST NOT
create a state where a continuation cell exists without a valid lead cell immediately to its left.

## Ops overview

- `zr_fb_init` / `zr_fb_release` — framebuffer lifetime; `zr_fb_init` allocates backing cells.
- `zr_fb_resize` — resizes backing store with a no-partial-effects contract (on failure, the framebuffer is unchanged).
- `zr_fb_cell` / `zr_fb_cell_const` — cell lookup helpers (return `NULL` if out of bounds).
- `zr_fb_painter_begin` — starts a painter with a caller-provided clip stack.
- `zr_fb_clip_push` / `zr_fb_clip_pop` — deterministic clip stack; effective clip is the intersection of framebuffer bounds
  and all pushed clips.
- `zr_fb_clear` / `zr_fb_fill_rect` — style-based fills.
- `zr_fb_draw_hline` / `zr_fb_draw_vline` / `zr_fb_draw_box` — ASCII line/box primitives.
- `zr_fb_draw_scrollbar_v` / `zr_fb_draw_scrollbar_h` — ASCII scrollbars.
- `zr_fb_put_grapheme` — writes a pre-segmented grapheme with an explicit width (`1` or `2`):
  - If `len > ZR_CELL_GLYPH_MAX`, writes `U+FFFD` width `1`.
  - If `width == 2` but cannot write both cells within bounds+clip, writes `U+FFFD` width `1` (never half glyphs).
- `zr_fb_draw_text_bytes` — draws UTF-8 text by iterating graphemes and calling `zr_fb_put_grapheme`.
- `zr_fb_blit_rect` — overlap-safe rectangle copy (memmove-like semantics) that preserves wide-glyph invariants.

## Layout and clipping

Clipping MUST NOT affect cursor advancement. When a wide glyph cannot be fully written within the current clip, the
draw is replaced with `U+FFFD` (width `1`) but the logical cursor advance remains `2`. This prevents clip-dependent text
shifts where subsequent glyphs move into/out of the visible region.

If clipping begins inside a continuation cell, the executor cannot safely reset the lead cell because it lies outside
the clip. In that case `zr_fb_put_grapheme()` aborts without modifying either cell to preserve wide-glyph invariants
(see `src/core/zr_framebuffer.c:446-516` and the regression in `tests/unit/test_clipping.c`). Wrappers should therefore not
expect a replacement glyph to appear when only continuation cells are writable.
