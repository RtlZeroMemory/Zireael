/*
  src/core/zr_debug_overlay.c — Deterministic, bounded debug overlay rendering.

  Why: Provides an internal overlay that can be drawn after drawlist execution
  and before diff emission without per-frame heap churn or invariant breaks.
*/

#include "core/zr_debug_overlay.h"

#include <stdbool.h>
#include <string.h>

static uint32_t zr_u32_min(uint32_t a, uint32_t b) {
  return (a < b) ? a : b;
}

static void zr_cell_set_ascii(zr_cell_t* cell, uint8_t ch, zr_style_t style) {
  if (!cell) {
    return;
  }
  memset(cell->glyph, 0, sizeof(cell->glyph));
  cell->glyph[0] = ch;
  cell->glyph_len = 1u;
  cell->width = 1u;
  cell->_pad0 = 0u;
  cell->style = style;
}

static bool zr_cell_is_continuation(const zr_cell_t* cell) {
  return cell && cell->width == 0u;
}

/*
 * Write a single ASCII cell while preserving wide-glyph continuation invariants.
 *
 * Why: Overlay uses width-1 glyphs; overwriting an existing wide glyph must
 * clear its paired continuation cell when both are within the overlay region,
 * and must skip writes that would split a wide glyph across the overlay boundary.
 */
static void zr_overlay_write_ascii_cell(zr_fb_t* fb, uint32_t x, uint32_t y, uint32_t overlay_cols, uint8_t ch,
                                        zr_style_t style) {
  zr_cell_t* c = zr_fb_cell(fb, x, y);
  if (!c) {
    return;
  }

  /* If we are about to write into a continuation cell, clear the lead cell too. */
  if (zr_cell_is_continuation(c)) {
    if (x == 0u) {
      return;
    }
    const uint32_t lead_x = x - 1u;
    if (lead_x >= overlay_cols) {
      return;
    }
    zr_cell_t* lead = zr_fb_cell(fb, lead_x, y);
    zr_cell_t* cont = c;
    if (!lead) {
      return;
    }
    zr_cell_set_ascii(lead, (uint8_t)' ', style);
    zr_cell_set_ascii(cont, (uint8_t)' ', style);
  }

  /* If we are overwriting a lead cell of a wide glyph, clear the continuation cell too. */
  if (x + 1u < fb->cols) {
    zr_cell_t* next = zr_fb_cell(fb, x + 1u, y);
    if (zr_cell_is_continuation(next)) {
      if (x + 1u >= overlay_cols) {
        /* Would split a wide glyph across the overlay boundary; leave it intact. */
        return;
      }
      zr_cell_set_ascii(next, (uint8_t)' ', style);
    }
  }

  zr_cell_set_ascii(c, ch, style);
}

static size_t zr_line_write_lit(char* dst, size_t cap, size_t off, const char* lit) {
  if (!dst || cap == 0u || !lit) {
    return off;
  }
  for (size_t i = 0u; lit[i] != '\0'; i++) {
    if (off >= cap) {
      return off;
    }
    dst[off++] = lit[i];
  }
  return off;
}

static size_t zr_line_write_u32_dec(char* dst, size_t cap, size_t off, uint32_t v) {
  char tmp[10];
  size_t n = 0u;
  do {
    tmp[n++] = (char)('0' + (char)(v % 10u));
    v /= 10u;
  } while (v != 0u && n < sizeof(tmp));

  while (n != 0u) {
    if (off >= cap) {
      return off;
    }
    dst[off++] = tmp[--n];
  }
  return off;
}

static void zr_build_line0(char* dst, size_t cap, const zr_metrics_t* m) {
  memset(dst, ' ', cap);
  size_t off = 0u;
  off = zr_line_write_lit(dst, cap, off, "FPS:");
  off = zr_line_write_u32_dec(dst, cap, off, m ? m->fps : 0u);
  off = zr_line_write_lit(dst, cap, off, "  BYTES:");
  (void)zr_line_write_u32_dec(dst, cap, off, m ? m->bytes_emitted_last_frame : 0u);
}

static void zr_build_line1(char* dst, size_t cap, const zr_metrics_t* m) {
  memset(dst, ' ', cap);
  size_t off = 0u;
  off = zr_line_write_lit(dst, cap, off, "DIRTY L:");
  off = zr_line_write_u32_dec(dst, cap, off, m ? m->dirty_lines_last_frame : 0u);
  off = zr_line_write_lit(dst, cap, off, " C:");
  (void)zr_line_write_u32_dec(dst, cap, off, m ? m->dirty_cols_last_frame : 0u);
}

static void zr_build_line2(char* dst, size_t cap, const zr_metrics_t* m) {
  memset(dst, ' ', cap);
  size_t off = 0u;
  off = zr_line_write_lit(dst, cap, off, "US in:");
  off = zr_line_write_u32_dec(dst, cap, off, m ? m->us_input_last_frame : 0u);
  off = zr_line_write_lit(dst, cap, off, " dl:");
  off = zr_line_write_u32_dec(dst, cap, off, m ? m->us_drawlist_last_frame : 0u);
  off = zr_line_write_lit(dst, cap, off, " df:");
  off = zr_line_write_u32_dec(dst, cap, off, m ? m->us_diff_last_frame : 0u);
  off = zr_line_write_lit(dst, cap, off, " wr:");
  (void)zr_line_write_u32_dec(dst, cap, off, m ? m->us_write_last_frame : 0u);
}

static void zr_build_line3(char* dst, size_t cap, const zr_metrics_t* m) {
  memset(dst, ' ', cap);
  size_t off = 0u;
  off = zr_line_write_lit(dst, cap, off, "EV out:");
  off = zr_line_write_u32_dec(dst, cap, off, m ? m->events_out_last_poll : 0u);
  off = zr_line_write_lit(dst, cap, off, " drop:");
  (void)zr_line_write_u32_dec(dst, cap, off, m ? m->events_dropped_total : 0u);
}

typedef void (*zr_overlay_build_fn_t)(char* dst, size_t cap, const zr_metrics_t* m);

static void zr_overlay_render_line(zr_fb_t* fb, uint32_t cols, uint32_t y, uint32_t overlay_cols,
                                   zr_overlay_build_fn_t build, char* line, const zr_metrics_t* metrics,
                                   zr_style_t style) {
  build(line, ZR_DEBUG_OVERLAY_MAX_COLS, metrics);
  for (uint32_t x = 0u; x < cols; x++) {
    zr_overlay_write_ascii_cell(fb, x, y, overlay_cols, (uint8_t)line[x], style);
  }
}

/* Render a 4x40 ASCII overlay, clipped to the framebuffer bounds. */
zr_result_t zr_debug_overlay_render(zr_fb_t* fb, const zr_metrics_t* metrics) {
  if (!fb || !metrics) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!fb->cells || fb->cols == 0u || fb->rows == 0u) {
    return ZR_OK;
  }

  const uint32_t rows = zr_u32_min(fb->rows, ZR_DEBUG_OVERLAY_MAX_ROWS);
  const uint32_t cols = zr_u32_min(fb->cols, ZR_DEBUG_OVERLAY_MAX_COLS);
  if (rows == 0u || cols == 0u) {
    return ZR_OK;
  }

  const zr_style_t style = (zr_style_t){0u, 0u, 0u, 0u, 0u, 0u};

  char line[ZR_DEBUG_OVERLAY_MAX_COLS];

  /* --- Line 0 --- */
  if (rows >= 1u) {
    zr_overlay_render_line(fb, cols, 0u, cols, zr_build_line0, line, metrics, style);
  }

  /* --- Line 1 --- */
  if (rows >= 2u) {
    zr_overlay_render_line(fb, cols, 1u, cols, zr_build_line1, line, metrics, style);
  }

  /* --- Line 2 --- */
  if (rows >= 3u) {
    zr_overlay_render_line(fb, cols, 2u, cols, zr_build_line2, line, metrics, style);
  }

  /* --- Line 3 --- */
  if (rows >= 4u) {
    zr_overlay_render_line(fb, cols, 3u, cols, zr_build_line3, line, metrics, style);
  }

  return ZR_OK;
}
