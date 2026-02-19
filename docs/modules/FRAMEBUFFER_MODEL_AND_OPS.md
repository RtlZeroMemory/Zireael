# Module — Framebuffer Model and Ops

This module documents the in-memory framebuffer used by drawlist execution and internal renderers.

Defined by `src/core/zr_framebuffer.h` / `src/core/zr_framebuffer.c`.

Compatibility shim: `src/core/zr_fb.h` includes `core/zr_framebuffer.h`.

## Data model

### Shared structs

- `zr_rect_t { int32_t x, y, w, h; }`
- `zr_style_t { uint32_t fg_rgb, bg_rgb, attrs, reserved, underline_rgb, link_ref; }`
  - `reserved` low bits carry underline variant in drawlist v3/style v3 (`0` keeps legacy underline behavior).
  - `underline_rgb` is underline color (`0x00RRGGBB`; `0` means terminal default underline color).
  - `link_ref` is a framebuffer-owned 1-based hyperlink reference (`0` means no link).

### Cells

Each cell (`zr_cell_t`) stores:

- `glyph[ZR_CELL_GLYPH_MAX]` — UTF-8 bytes for a **single grapheme**
- `glyph_len` — number of valid bytes in `glyph`
- `style` — foreground/background/attributes (`zr_style_t`)
- `width` — one of:
  - `1` for a normal cell
  - `2` for a wide lead cell (spans this cell + the next cell)
  - `0` for a wide continuation cell (must be empty)

### Hyperlink table

Framebuffer hyperlink references are interned in framebuffer-owned storage:

- `zr_fb_link_t { uri_off, uri_len, id_off, id_len }` points into `fb.link_bytes`
- `zr_fb_link_intern()` deduplicates `(uri,id)` pairs and returns stable 1-based refs
- `zr_fb_link_lookup()` resolves refs for OSC 8 emission in the diff renderer
- `zr_fb_links_reset()` clears per-frame link usage without reallocating buffers
- `zr_fb_links_clone_from()` copies link tables between framebuffers for deterministic model replay/tests

Limits:

- URI max bytes: `ZR_FB_LINK_URI_MAX_BYTES` (2083)
- ID max bytes: `ZR_FB_LINK_ID_MAX_BYTES` (2083)

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
- `zr_fb_link_intern` / `zr_fb_link_lookup` / `zr_fb_links_reset` / `zr_fb_links_clone_from` — hyperlink table helpers.
- `zr_fb_painter_begin` — starts a painter with a caller-provided clip stack.
- `zr_fb_clip_push` / `zr_fb_clip_pop` — deterministic clip stack; effective clip is the intersection of framebuffer bounds
  and all pushed clips.
- `zr_fb_clear` / `zr_fb_fill_rect` — style-based fills.
- `zr_fb_draw_hline` / `zr_fb_draw_vline` / `zr_fb_draw_box` — ASCII line/box primitives.
- `zr_fb_draw_scrollbar_v` / `zr_fb_draw_scrollbar_h` — ASCII scrollbars.
- `zr_fb_put_grapheme` — writes a pre-segmented grapheme with an explicit width (`1` or `2`):
  - If `len == 0`, normalizes to a single ASCII space (width `1`).
  - If `len > ZR_CELL_GLYPH_MAX`, writes `U+FFFD` width `1`.
  - If bytes contain invalid UTF-8 or Unicode control scalars (C0/DEL/C1), writes `U+FFFD` width `1` to prevent
    emitting terminal control bytes.
  - If `width == 2` but cannot write both cells within bounds+clip, writes `U+FFFD` width `1` (never half glyphs).
  - Invariant-repair exception (LOCKED): when overwriting a continuation/lead edge, it may clear exactly one adjacent
    paired cell (`x-1` or `x+1`) outside clip to prevent orphan wide-pair state.
- `zr_fb_draw_text_bytes` — draws UTF-8 text by iterating graphemes and calling `zr_fb_put_grapheme`.
- `zr_fb_blit_rect` — overlap-safe rectangle copy (memmove-like semantics) that preserves wide-glyph invariants.

## Layout and clipping

Clipping MUST NOT affect cursor advancement. When a wide glyph cannot be fully written within the current clip, the
draw is replaced with `U+FFFD` (width `1`) but the logical cursor advance remains `2`. This prevents clip-dependent text
shifts where subsequent glyphs move into/out of the visible region.

If clipping begins inside a continuation cell (or covers only a wide lead), `zr_fb_put_grapheme()` applies a bounded
invariant-repair exception: it may clear the paired cell immediately adjacent to the target (`x-1` or `x+1`) even when
that paired cell is outside clip. This is the only allowed out-of-clip mutation and prevents stale orphan lead/continuation
pairs on incremental frames (see `tests/unit/test_clipping.c`).
