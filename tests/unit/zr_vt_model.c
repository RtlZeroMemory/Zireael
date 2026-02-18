/*
  tests/unit/zr_vt_model.c — Minimal VT/ANSI output model for renderer tests.

  Why: Applying renderer-emitted bytes in a tiny model lets tests catch state
  drift bugs (cursor/style mismatches, incomplete clears) without depending on
  a real terminal or timing.
*/

#include "unit/zr_vt_model.h"

#include "unicode/zr_grapheme.h"
#include "unicode/zr_width.h"

#include <stdbool.h>
#include <string.h>

static zr_style_t zr_style_default(void) {
  zr_style_t s;
  s.fg_rgb = 0u;
  s.bg_rgb = 0u;
  s.attrs = 0u;
  s.reserved = 0u;
  return s;
}

static bool zr_vt_model_cursor_pos_is_valid(const zr_vt_model_t* m) {
  return m && ((m->ts.flags & ZR_TERM_STATE_CURSOR_POS_VALID) != 0u);
}

static void zr_vt_model_home_cursor(zr_vt_model_t* m) {
  if (!m) {
    return;
  }
  m->ts.cursor_x = 0u;
  m->ts.cursor_y = 0u;
  m->ts.flags |= ZR_TERM_STATE_CURSOR_POS_VALID;
}

static void zr_vt_model_set_scroll_region(zr_vt_model_t* m, uint32_t top, uint32_t bottom) {
  if (!m || m->rows == 0u) {
    return;
  }
  if (top >= m->rows) {
    top = m->rows - 1u;
  }
  if (bottom >= m->rows) {
    bottom = m->rows - 1u;
  }
  if (bottom < top) {
    top = 0u;
    bottom = m->rows - 1u;
  }
  m->scroll_top = top;
  m->scroll_bottom = bottom;

  /* xterm/VT behavior: DECSTBM homes cursor. */
  zr_vt_model_home_cursor(m);
}

static void zr_vt_model_reset_scroll_region(zr_vt_model_t* m) {
  if (!m || m->rows == 0u) {
    return;
  }
  zr_vt_model_set_scroll_region(m, 0u, m->rows - 1u);
}

static zr_result_t zr_vt_model_fill_rows(zr_vt_model_t* m, uint32_t y0, uint32_t y1_incl, zr_style_t style) {
  if (!m) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (m->cols == 0u || m->rows == 0u) {
    return ZR_OK;
  }
  if (y0 > y1_incl || y0 >= m->rows) {
    return ZR_OK;
  }
  if (y1_incl >= m->rows) {
    y1_incl = m->rows - 1u;
  }

  zr_rect_t r;
  r.x = 0;
  r.y = (int32_t)y0;
  r.w = (int32_t)m->cols;
  r.h = (int32_t)(y1_incl - y0 + 1u);
  return zr_fb_fill_rect(&m->painter, r, &style);
}

static zr_result_t zr_vt_model_scroll(zr_vt_model_t* m, bool up, uint32_t lines) {
  if (!m) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (m->cols == 0u || m->rows == 0u || !m->screen.cells) {
    return ZR_OK;
  }
  if (lines == 0u) {
    return ZR_OK;
  }
  if (m->scroll_top >= m->rows || m->scroll_bottom >= m->rows || m->scroll_bottom < m->scroll_top) {
    zr_vt_model_reset_scroll_region(m);
  }

  const uint32_t top = m->scroll_top;
  const uint32_t bottom = m->scroll_bottom;
  const uint32_t height = bottom - top + 1u;
  if (lines >= height) {
    return zr_vt_model_fill_rows(m, top, bottom, m->ts.style);
  }

  const size_t row_bytes = (size_t)m->cols * sizeof(zr_cell_t);
  if (row_bytes == 0u) {
    return ZR_OK;
  }

  zr_cell_t* const base = m->screen.cells;
  if (up) {
    zr_cell_t* dst = base + (size_t)top * (size_t)m->cols;
    zr_cell_t* src = base + (size_t)(top + lines) * (size_t)m->cols;
    const size_t move_rows = (size_t)(height - lines);
    memmove(dst, src, move_rows * row_bytes);
    return zr_vt_model_fill_rows(m, bottom - lines + 1u, bottom, m->ts.style);
  }

  zr_cell_t* dst = base + (size_t)(top + lines) * (size_t)m->cols;
  zr_cell_t* src = base + (size_t)top * (size_t)m->cols;
  const size_t move_rows = (size_t)(height - lines);
  memmove(dst, src, move_rows * row_bytes);
  return zr_vt_model_fill_rows(m, top, top + lines - 1u, m->ts.style);
}

static bool zr_vt_model_glyph_may_drift_cursor(const uint8_t* bytes, size_t len, uint8_t width) {
  if (width != 1u) {
    return true;
  }
  if (!bytes || len == 0u) {
    return false;
  }
  for (size_t i = 0u; i < len; i++) {
    if (bytes[i] >= 0x80u) {
      return true;
    }
  }
  return false;
}

static zr_result_t zr_vt_model_print_utf8(zr_vt_model_t* m, const uint8_t* bytes, size_t len) {
  if (!m || (!bytes && len != 0u)) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (len == 0u) {
    return ZR_OK;
  }
  if (m->cols == 0u || m->rows == 0u) {
    return ZR_OK;
  }
  if (!zr_vt_model_cursor_pos_is_valid(m)) {
    /* Printing without an anchored cursor position is a renderer bug. */
    return ZR_ERR_FORMAT;
  }

  zr_grapheme_iter_t it;
  zr_grapheme_iter_init(&it, bytes, len);

  zr_grapheme_t g;
  while (zr_grapheme_next(&it, &g)) {
    if (!zr_vt_model_cursor_pos_is_valid(m)) {
      /* Cursor drift requires CUP before printing the next cell. */
      return ZR_ERR_FORMAT;
    }
    const uint8_t* gb = bytes + g.offset;
    const size_t gl = g.size;
    const uint8_t w = zr_width_grapheme_utf8(gb, gl, zr_width_policy_default());
    if (w == 0u) {
      continue;
    }

    (void)zr_fb_put_grapheme(&m->painter, (int32_t)m->ts.cursor_x, (int32_t)m->ts.cursor_y, gb, gl, w, &m->ts.style);

    m->ts.cursor_x += (uint32_t)w;
    if (zr_vt_model_glyph_may_drift_cursor(gb, gl, w)) {
      m->ts.flags &= (uint8_t)~ZR_TERM_STATE_CURSOR_POS_VALID;
    }
  }

  return ZR_OK;
}

static uint32_t zr_vt_parse_u32(const uint8_t* bytes, size_t len, size_t* inout_off, bool* out_any) {
  if (out_any) {
    *out_any = false;
  }
  if (!bytes || !inout_off) {
    return 0u;
  }

  uint32_t v = 0u;
  bool any = false;
  while (*inout_off < len) {
    const uint8_t ch = bytes[*inout_off];
    if (ch < (uint8_t)'0' || ch > (uint8_t)'9') {
      break;
    }
    any = true;
    const uint32_t digit = (uint32_t)(ch - (uint8_t)'0');
    if (v > (UINT32_MAX - digit) / 10u) {
      v = UINT32_MAX;
    } else {
      v = v * 10u + digit;
    }
    (*inout_off)++;
  }

  if (out_any) {
    *out_any = any;
  }
  return v;
}

static void zr_vt_model_apply_sgr(zr_vt_model_t* m, const uint32_t* params, size_t count) {
  if (!m) {
    return;
  }

  if (!params || count == 0u) {
    /* Empty SGR is equivalent to reset. */
    m->ts.style = zr_style_default();
    m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
    return;
  }

  size_t i = 0u;
  while (i < count) {
    const uint32_t p = params[i++];
    if (p == 0u) {
      m->ts.style = zr_style_default();
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }

    /* --- Attributes (add-only; renderer uses reset for clears) --- */
    if (p == 1u) {
      m->ts.style.attrs |= (1u << 0);
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }
    if (p == 2u) {
      m->ts.style.attrs |= (1u << 4);
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }
    if (p == 3u) {
      m->ts.style.attrs |= (1u << 1);
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }
    if (p == 4u) {
      m->ts.style.attrs |= (1u << 2);
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }
    if (p == 5u) {
      m->ts.style.attrs |= (1u << 7);
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }
    if (p == 7u) {
      m->ts.style.attrs |= (1u << 3);
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }
    if (p == 9u) {
      m->ts.style.attrs |= (1u << 5);
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }
    if (p == 53u) {
      m->ts.style.attrs |= (1u << 6);
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }

    /* --- 16-color (ANSI) --- */
    if (p >= 30u && p <= 37u) {
      m->ts.style.fg_rgb = (uint32_t)(p - 30u);
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }
    if (p >= 90u && p <= 97u) {
      m->ts.style.fg_rgb = (uint32_t)(8u + (p - 90u));
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }
    if (p >= 40u && p <= 47u) {
      m->ts.style.bg_rgb = (uint32_t)(p - 40u);
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }
    if (p >= 100u && p <= 107u) {
      m->ts.style.bg_rgb = (uint32_t)(8u + (p - 100u));
      m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      continue;
    }

    /* --- Extended colors --- */
    if (p == 38u || p == 48u) {
      const bool fg = (p == 38u);
      if (i >= count) {
        break;
      }
      const uint32_t mode = params[i++];
      if (mode == 2u) {
        if (i + 2u >= count) {
          break;
        }
        const uint32_t r = params[i++];
        const uint32_t g = params[i++];
        const uint32_t b = params[i++];
        const uint32_t rgb = ((r & 0xFFu) << 16) | ((g & 0xFFu) << 8) | (b & 0xFFu);
        if (fg) {
          m->ts.style.fg_rgb = rgb;
        } else {
          m->ts.style.bg_rgb = rgb;
        }
        m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      } else if (mode == 5u) {
        if (i >= count) {
          break;
        }
        const uint32_t idx = params[i++] & 0xFFu;
        if (fg) {
          m->ts.style.fg_rgb = idx;
        } else {
          m->ts.style.bg_rgb = idx;
        }
        m->ts.flags |= ZR_TERM_STATE_STYLE_VALID;
      }
      continue;
    }
  }
}

static void zr_vt_model_apply_cursor_vis(zr_vt_model_t* m, uint8_t visible) {
  if (!m) {
    return;
  }
  m->ts.cursor_visible = visible;
  m->ts.flags |= ZR_TERM_STATE_CURSOR_VIS_VALID;
}

static void zr_vt_model_apply_cursor_shape(zr_vt_model_t* m, uint32_t ps) {
  if (!m) {
    return;
  }

  uint8_t shape = 0u;
  uint8_t blink = 0u;
  switch (ps) {
  case 1u:
    shape = ZR_CURSOR_SHAPE_BLOCK;
    blink = 1u;
    break;
  case 2u:
    shape = ZR_CURSOR_SHAPE_BLOCK;
    blink = 0u;
    break;
  case 3u:
    shape = ZR_CURSOR_SHAPE_UNDERLINE;
    blink = 1u;
    break;
  case 4u:
    shape = ZR_CURSOR_SHAPE_UNDERLINE;
    blink = 0u;
    break;
  case 5u:
    shape = ZR_CURSOR_SHAPE_BAR;
    blink = 1u;
    break;
  case 6u:
    shape = ZR_CURSOR_SHAPE_BAR;
    blink = 0u;
    break;
  default:
    return;
  }

  m->ts.cursor_shape = shape;
  m->ts.cursor_blink = blink;
  m->ts.flags |= ZR_TERM_STATE_CURSOR_SHAPE_VALID;
}

zr_result_t zr_vt_model_init(zr_vt_model_t* m, uint32_t cols, uint32_t rows) {
  if (!m || cols == 0u || rows == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  memset(m, 0, sizeof(*m));

  zr_result_t rc = zr_fb_init(&m->screen, cols, rows);
  if (rc != ZR_OK) {
    return rc;
  }

  m->cols = cols;
  m->rows = rows;
  rc = zr_fb_painter_begin(&m->painter, &m->screen, m->clip_stack, 2u);
  if (rc != ZR_OK) {
    zr_fb_release(&m->screen);
    memset(m, 0, sizeof(*m));
    return rc;
  }

  zr_vt_model_reset_scroll_region(m);
  m->ts.style = zr_style_default();
  return ZR_OK;
}

void zr_vt_model_release(zr_vt_model_t* m) {
  if (!m) {
    return;
  }
  zr_fb_release(&m->screen);
  memset(m, 0, sizeof(*m));
}

zr_result_t zr_vt_model_reset(zr_vt_model_t* m, const zr_fb_t* screen, const zr_term_state_t* ts) {
  if (!m || !ts) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (m->cols == 0u || m->rows == 0u || !m->screen.cells) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (screen) {
    if (screen->cols != m->cols || screen->rows != m->rows || !screen->cells) {
      return ZR_ERR_INVALID_ARGUMENT;
    }
    const size_t n = (size_t)m->cols * (size_t)m->rows * sizeof(zr_cell_t);
    memcpy(m->screen.cells, screen->cells, n);
  } else {
    (void)zr_fb_clear(&m->screen, &ts->style);
  }

  m->ts = *ts;
  zr_vt_model_reset_scroll_region(m);
  return ZR_OK;
}

zr_result_t zr_vt_model_apply(zr_vt_model_t* m, const uint8_t* bytes, size_t len) {
  if (!m || (!bytes && len != 0u)) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  size_t i = 0u;
  while (i < len) {
    const uint8_t ch = bytes[i];
    if (ch != 0x1Bu) {
      /* Consume until next ESC as one printable run. */
      size_t j = i;
      while (j < len && bytes[j] != 0x1Bu) {
        j++;
      }
      const zr_result_t rc = zr_vt_model_print_utf8(m, bytes + i, j - i);
      if (rc != ZR_OK) {
        return rc;
      }
      i = j;
      continue;
    }

    /* CSI only: ESC [ ... */
    if (i + 1u >= len || bytes[i + 1u] != (uint8_t)'[') {
      return ZR_ERR_FORMAT;
    }
    i += 2u;

    bool priv = false;
    if (i < len && bytes[i] == (uint8_t)'?') {
      priv = true;
      i++;
    }

    uint32_t params[32];
    size_t param_count = 0u;
    bool in_params = true;
    bool last_was_sep = true;

    while (i < len && in_params) {
      const uint8_t c = bytes[i];
      if ((c >= (uint8_t)'0' && c <= (uint8_t)'9') || c == (uint8_t)';') {
        if (c == (uint8_t)';') {
          if (param_count < (sizeof(params) / sizeof(params[0]))) {
            if (last_was_sep) {
              params[param_count++] = 0u;
            }
          }
          last_was_sep = true;
          i++;
          continue;
        }

        bool any = false;
        const uint32_t v = zr_vt_parse_u32(bytes, len, &i, &any);
        if (any) {
          if (param_count < (sizeof(params) / sizeof(params[0]))) {
            params[param_count++] = v;
          }
          last_was_sep = false;
          continue;
        }
      }
      in_params = false;
    }

    uint8_t intermediate = 0u;
    if (i < len && bytes[i] >= 0x20u && bytes[i] <= 0x2Fu) {
      intermediate = bytes[i];
      i++;
    }
    if (i >= len) {
      return ZR_ERR_FORMAT;
    }
    const uint8_t final = bytes[i++];

    /* --- Dispatch supported sequences --- */
    if (!priv && intermediate == 0u && final == (uint8_t)'H') {
      const uint32_t row = (param_count >= 1u && params[0] != 0u) ? params[0] : 1u;
      const uint32_t col = (param_count >= 2u && params[1] != 0u) ? params[1] : 1u;
      m->ts.cursor_x = (col > 0u) ? (col - 1u) : 0u;
      m->ts.cursor_y = (row > 0u) ? (row - 1u) : 0u;
      m->ts.flags |= ZR_TERM_STATE_CURSOR_POS_VALID;
      continue;
    }

    if (!priv && intermediate == 0u && final == (uint8_t)'m') {
      zr_vt_model_apply_sgr(m, params, param_count);
      continue;
    }

    if (!priv && intermediate == 0u && final == (uint8_t)'r') {
      if (param_count >= 2u && params[0] != 0u && params[1] != 0u) {
        zr_vt_model_set_scroll_region(m, params[0] - 1u, params[1] - 1u);
      } else {
        zr_vt_model_reset_scroll_region(m);
      }
      continue;
    }

    if (!priv && intermediate == 0u && (final == (uint8_t)'S' || final == (uint8_t)'T')) {
      const uint32_t lines = (param_count >= 1u && params[0] != 0u) ? params[0] : 1u;
      const zr_result_t rc = zr_vt_model_scroll(m, final == (uint8_t)'S', lines);
      if (rc != ZR_OK) {
        return rc;
      }
      continue;
    }

    if (!priv && intermediate == 0u && final == (uint8_t)'J') {
      const uint32_t mode = (param_count >= 1u) ? params[0] : 0u;
      if (mode == 2u) {
        (void)zr_fb_clear(&m->screen, &m->ts.style);
        m->ts.flags |= ZR_TERM_STATE_SCREEN_VALID;
      }
      continue;
    }

    if (priv && intermediate == 0u && (final == (uint8_t)'h' || final == (uint8_t)'l')) {
      if (param_count >= 1u && params[0] == 25u) {
        zr_vt_model_apply_cursor_vis(m, (final == (uint8_t)'h') ? 1u : 0u);
      }
      /* Ignore other private modes (sync update, etc.). */
      continue;
    }

    if (!priv && intermediate == (uint8_t)' ' && final == (uint8_t)'q') {
      if (param_count >= 1u) {
        zr_vt_model_apply_cursor_shape(m, params[0]);
      }
      continue;
    }

    /* Unknown CSI: ignore. */
  }

  return ZR_OK;
}
