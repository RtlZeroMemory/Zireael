/*
  src/core/zr_fb.h — Minimal in-memory framebuffer model (core-internal).

  Why: Provides a deterministic, OS-header-free surface for drawlist execution
  to write cells into an offscreen grid.
*/

#ifndef ZR_CORE_ZR_FB_H_INCLUDED
#define ZR_CORE_ZR_FB_H_INCLUDED

#include "util/zr_result.h"

#include <stddef.h>
#include <stdint.h>

typedef struct zr_style_t {
  uint32_t fg;
  uint32_t bg;
  uint32_t attrs;
} zr_style_t;

typedef struct zr_fb_cell_t {
  /*
    glyph:
      - UTF-8 bytes for a single codepoint (1..4 bytes) as emitted/consumed by
        the framebuffer's decode policy.
      - Note: this is a placeholder until full grapheme-aware rendering lands.
  */
  uint8_t   glyph[4];
  uint8_t   glyph_len;
  uint8_t   flags;
  uint16_t  _pad0;
  zr_style_t style;
} zr_fb_cell_t;

#define ZR_FB_CELL_FLAG_CONTINUATION ((uint8_t)0x01u)

typedef struct zr_fb_t {
  uint32_t     cols;
  uint32_t     rows;
  /*
    cells:
      - Caller-owned backing store; zr_fb_init() never allocates.
      - Length is exactly cols*rows (in row-major order).
      - Must remain valid for the lifetime of the zr_fb_t.
  */
  zr_fb_cell_t* cells;
} zr_fb_t;

typedef struct zr_fb_rect_i32_t {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
} zr_fb_rect_i32_t;

zr_result_t zr_fb_init(zr_fb_t* fb, zr_fb_cell_t* backing, uint32_t cols, uint32_t rows);
zr_result_t zr_fb_clear(zr_fb_t* fb, const zr_style_t* style);
zr_fb_cell_t* zr_fb_cell_at(zr_fb_t* fb, uint32_t x, uint32_t y);
const zr_fb_cell_t* zr_fb_cell_at_const(const zr_fb_t* fb, uint32_t x, uint32_t y);

zr_result_t zr_fb_fill_rect(zr_fb_t* fb, zr_fb_rect_i32_t r, const zr_style_t* style,
                            zr_fb_rect_i32_t clip);
zr_result_t zr_fb_draw_text_bytes(zr_fb_t* fb, int32_t x, int32_t y, const uint8_t* bytes,
                                  size_t len, const zr_style_t* style, zr_fb_rect_i32_t clip);

/* Deterministic UTF-8 cell count using the framebuffer's decode policy. */
size_t zr_fb_count_cells_utf8(const uint8_t* bytes, size_t len);

zr_fb_rect_i32_t zr_fb_full_clip(const zr_fb_t* fb);
zr_fb_rect_i32_t zr_fb_clip_intersect(zr_fb_rect_i32_t a, zr_fb_rect_i32_t b);

#endif /* ZR_CORE_ZR_FB_H_INCLUDED */
