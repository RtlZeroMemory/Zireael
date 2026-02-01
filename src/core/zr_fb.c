/*
  src/core/zr_fb.c — In-memory framebuffer with grapheme-aware text rendering.

  Why: Provides a deterministic, OS-header-free surface for drawlist execution.
  Text is rendered at grapheme cluster boundaries with proper width handling
  for wide characters (CJK, emoji) using continuation cells.
*/

#include "core/zr_fb.h"

#include "unicode/zr_grapheme.h"
#include "unicode/zr_utf8.h"
#include "unicode/zr_width.h"

#include "util/zr_checked.h"

#include <limits.h>
#include <string.h>

static zr_style_t zr_style_default(void) {
  zr_style_t s;
  s.fg = 0u;
  s.bg = 0u;
  s.attrs = 0u;
  return s;
}

static bool zr_fb_has_backing(const zr_fb_t* fb) {
  return fb && fb->cells && fb->cols != 0u && fb->rows != 0u;
}

/* Convert (x,y) coordinates to linear cell index with overflow-safe arithmetic. */
static bool zr_fb_cell_index(const zr_fb_t* fb, uint32_t x, uint32_t y, size_t* out_idx) {
  if (!out_idx) {
    return false;
  }
  if (!zr_fb_has_backing(fb)) {
    return false;
  }
  if (x >= fb->cols || y >= fb->rows) {
    return false;
  }

  /* idx = (y * cols) + x; do the math with checked helpers. */
  size_t row_start = 0u;
  if (!zr_checked_mul_size((size_t)y, (size_t)fb->cols, &row_start)) {
    return false;
  }
  if (!zr_checked_add_size(row_start, (size_t)x, &row_start)) {
    return false;
  }
  *out_idx = row_start;
  return true;
}

static void zr_fb_cell_set_space(zr_fb_cell_t* cell, zr_style_t style) {
  if (!cell) {
    return;
  }
  memset(cell->glyph, 0, sizeof(cell->glyph));
  cell->glyph[0] = (uint8_t)' ';
  cell->glyph_len = 1u;
  cell->flags = 0u;
  cell->style = style;
}

/* Store a grapheme cluster in a cell; truncates to ZR_FB_GLYPH_MAX_BYTES. */
static void zr_fb_cell_set_glyph(zr_fb_cell_t* cell, const uint8_t* glyph, size_t glyph_len,
                                 zr_style_t style) {
  if (!cell) {
    return;
  }
  memset(cell->glyph, 0, sizeof(cell->glyph));
  if (glyph && glyph_len != 0u) {
    const size_t copy_len = (glyph_len <= ZR_FB_GLYPH_MAX_BYTES) ? glyph_len : ZR_FB_GLYPH_MAX_BYTES;
    memcpy(cell->glyph, glyph, copy_len);
    cell->glyph_len = (uint8_t)copy_len;
  } else {
    cell->glyph_len = 0u;
  }
  cell->flags = 0u;
  cell->style = style;
}

/* Mark a cell as a continuation of a wide character. */
static void zr_fb_cell_set_continuation(zr_fb_cell_t* cell, zr_style_t style) {
  if (!cell) {
    return;
  }
  memset(cell->glyph, 0, sizeof(cell->glyph));
  cell->glyph_len = 0u;
  cell->flags = ZR_FB_CELL_FLAG_CONTINUATION;
  cell->style = style;
}

zr_result_t zr_fb_init(zr_fb_t* fb, zr_fb_cell_t* backing, uint32_t cols, uint32_t rows) {
  if (!fb || (cols != 0u && rows != 0u && !backing)) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (cols > (uint32_t)INT32_MAX || rows > (uint32_t)INT32_MAX) {
    return ZR_ERR_LIMIT;
  }
  fb->cols = cols;
  fb->rows = rows;
  fb->cells = (cols != 0u && rows != 0u) ? backing : NULL;
  return ZR_OK;
}

zr_fb_rect_i32_t zr_fb_full_clip(const zr_fb_t* fb) {
  zr_fb_rect_i32_t r;
  r.x = 0;
  r.y = 0;
  r.w = fb ? (int32_t)fb->cols : 0;
  r.h = fb ? (int32_t)fb->rows : 0;
  return r;
}

static int32_t zr_i32_max(int32_t a, int32_t b) { return (a > b) ? a : b; }
static int64_t zr_i64_min(int64_t a, int64_t b) { return (a < b) ? a : b; }

/* Compute the intersection of two clip rectangles; returns empty rect if no overlap. */
zr_fb_rect_i32_t zr_fb_clip_intersect(zr_fb_rect_i32_t a, zr_fb_rect_i32_t b) {
  const int64_t ax2 = (a.w > 0) ? ((int64_t)a.x + (int64_t)a.w) : (int64_t)a.x;
  const int64_t ay2 = (a.h > 0) ? ((int64_t)a.y + (int64_t)a.h) : (int64_t)a.y;
  const int64_t bx2 = (b.w > 0) ? ((int64_t)b.x + (int64_t)b.w) : (int64_t)b.x;
  const int64_t by2 = (b.h > 0) ? ((int64_t)b.y + (int64_t)b.h) : (int64_t)b.y;

  zr_fb_rect_i32_t r;
  const int32_t x1 = zr_i32_max(a.x, b.x);
  const int32_t y1 = zr_i32_max(a.y, b.y);
  const int64_t x2 = zr_i64_min(ax2, bx2);
  const int64_t y2 = zr_i64_min(ay2, by2);

  r.x = x1;
  r.y = y1;

  int64_t w = x2 - (int64_t)x1;
  int64_t h = y2 - (int64_t)y1;
  if (w < 0) {
    w = 0;
  }
  if (h < 0) {
    h = 0;
  }
  if (w > (int64_t)INT32_MAX) {
    w = (int64_t)INT32_MAX;
  }
  if (h > (int64_t)INT32_MAX) {
    h = (int64_t)INT32_MAX;
  }
  r.w = (int32_t)w;
  r.h = (int32_t)h;
  return r;
}

static bool zr_fb_in_clip(int32_t x, int32_t y, zr_fb_rect_i32_t clip) {
  if (clip.w <= 0 || clip.h <= 0) {
    return false;
  }
  if (x < clip.x || y < clip.y) {
    return false;
  }
  const int64_t x2 = (int64_t)clip.x + (int64_t)clip.w;
  const int64_t y2 = (int64_t)clip.y + (int64_t)clip.h;
  if ((int64_t)x >= x2 || (int64_t)y >= y2) {
    return false;
  }
  return true;
}

zr_fb_cell_t* zr_fb_cell_at(zr_fb_t* fb, uint32_t x, uint32_t y) {
  size_t idx = 0u;
  if (!zr_fb_cell_index(fb, x, y, &idx)) {
    return NULL;
  }
  return &fb->cells[idx];
}

const zr_fb_cell_t* zr_fb_cell_at_const(const zr_fb_t* fb, uint32_t x, uint32_t y) {
  size_t idx = 0u;
  if (!zr_fb_cell_index(fb, x, y, &idx)) {
    return NULL;
  }
  return &fb->cells[idx];
}

zr_result_t zr_fb_clear(zr_fb_t* fb, const zr_style_t* style) {
  if (!fb) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!zr_fb_has_backing(fb)) {
    return ZR_OK;
  }
  size_t total = 0u;
  if (!zr_checked_mul_size((size_t)fb->cols, (size_t)fb->rows, &total)) {
    return ZR_ERR_LIMIT;
  }
  const zr_style_t s = style ? *style : zr_style_default();
  for (size_t i = 0u; i < total; i++) {
    zr_fb_cell_set_space(&fb->cells[i], s);
  }
  return ZR_OK;
}

/* Fill a rectangle with spaces in the given style, respecting clip bounds. */
zr_result_t zr_fb_fill_rect(zr_fb_t* fb, zr_fb_rect_i32_t r, const zr_style_t* style,
                            zr_fb_rect_i32_t clip) {
  if (!fb || !style) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (r.w <= 0 || r.h <= 0) {
    return ZR_OK;
  }
  if (!zr_fb_has_backing(fb)) {
    return ZR_OK;
  }
  zr_fb_rect_i32_t full = zr_fb_full_clip(fb);
  clip = zr_fb_clip_intersect(clip, full);
  zr_fb_rect_i32_t rr = zr_fb_clip_intersect(r, full);

  const int64_t y_end = (int64_t)rr.y + (int64_t)rr.h;
  const int64_t x_end = (int64_t)rr.x + (int64_t)rr.w;
  for (int64_t yy = (int64_t)rr.y; yy < y_end; yy++) {
    for (int64_t xx = (int64_t)rr.x; xx < x_end; xx++) {
      const int32_t y32 = (int32_t)yy;
      const int32_t x32 = (int32_t)xx;
      if (!zr_fb_in_clip(x32, y32, clip)) {
        continue;
      }
      zr_fb_cell_t* c = zr_fb_cell_at(fb, (uint32_t)x32, (uint32_t)y32);
      if (!c) {
        continue;
      }
      zr_fb_cell_set_space(c, *style);
    }
  }
  return ZR_OK;
}

/* U+FFFD replacement character in UTF-8. */
static const uint8_t ZR_UTF8_REPLACEMENT[] = {0xEFu, 0xBFu, 0xBDu};
#define ZR_UTF8_REPLACEMENT_LEN 3u

/*
 * Count terminal column width of a UTF-8 string using grapheme iteration.
 * Each grapheme contributes its display width (0, 1, or 2 columns).
 */
size_t zr_fb_count_cells_utf8(const uint8_t* bytes, size_t len) {
  if (!bytes || len == 0u) {
    return 0u;
  }

  size_t total_width = 0u;
  zr_grapheme_iter_t it;
  zr_grapheme_iter_init(&it, bytes, len);

  zr_grapheme_t g;
  while (zr_grapheme_next(&it, &g)) {
    const uint8_t w = zr_width_grapheme_utf8(bytes + g.offset, g.size, zr_width_policy_default());
    total_width += (size_t)w;
  }

  return total_width;
}

static bool zr_fb_can_draw_at(const zr_fb_t* fb, int64_t x, int32_t y, zr_fb_rect_i32_t clip) {
  if (!zr_fb_has_backing(fb)) {
    return false;
  }
  if (x < 0 || y < 0) {
    return false;
  }
  if (x >= (int64_t)fb->cols || (int64_t)y >= (int64_t)fb->rows) {
    return false;
  }
  return zr_fb_in_clip((int32_t)x, y, clip);
}

/*
 * Draw UTF-8 text at (x,y) using grapheme-aware iteration.
 *
 * Each grapheme cluster occupies 0, 1, or 2 cells based on its display width.
 * Zero-width graphemes (controls, extend-only, ZWJ-only) are skipped entirely.
 * Wide characters (width=2) use a lead cell followed by a continuation cell.
 * Graphemes exceeding ZR_FB_GLYPH_MAX_BYTES are replaced with U+FFFD.
 */
zr_result_t zr_fb_draw_text_bytes(zr_fb_t* fb, int32_t x, int32_t y, const uint8_t* bytes,
                                  size_t len, const zr_style_t* style, zr_fb_rect_i32_t clip) {
  if (!fb || !bytes || !style) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!zr_fb_has_backing(fb)) {
    return ZR_OK;
  }

  zr_fb_rect_i32_t full = zr_fb_full_clip(fb);
  clip = zr_fb_clip_intersect(clip, full);

  int64_t cx = (int64_t)x;
  zr_grapheme_iter_t it;
  zr_grapheme_iter_init(&it, bytes, len);

  zr_grapheme_t g;
  while (zr_grapheme_next(&it, &g)) {
    const uint8_t* glyph_bytes = bytes + g.offset;
    size_t glyph_len = g.size;

    /* Replace oversized graphemes with U+FFFD. */
    if (glyph_len > ZR_FB_GLYPH_MAX_BYTES) {
      glyph_bytes = ZR_UTF8_REPLACEMENT;
      glyph_len = ZR_UTF8_REPLACEMENT_LEN;
    }

    const uint8_t width = zr_width_grapheme_utf8(glyph_bytes, glyph_len, zr_width_policy_default());

    /* Skip zero-width graphemes (controls, extend-only, ZWJ-only clusters). */
    if (width == 0u) {
      continue;
    }

    /* --- Draw lead cell --- */
    if (zr_fb_can_draw_at(fb, cx, y, clip)) {
      zr_fb_cell_t* c = zr_fb_cell_at(fb, (uint32_t)cx, (uint32_t)y);
      if (c) {
        zr_fb_cell_set_glyph(c, glyph_bytes, glyph_len, *style);
      }
    }
    cx++;

    /* --- Draw continuation cell for wide characters --- */
    if (width == 2u) {
      if (zr_fb_can_draw_at(fb, cx, y, clip)) {
        zr_fb_cell_t* c = zr_fb_cell_at(fb, (uint32_t)cx, (uint32_t)y);
        if (c) {
          zr_fb_cell_set_continuation(c, *style);
        }
      }
      cx++;
    }
  }

  return ZR_OK;
}
