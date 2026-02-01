/*
  src/core/zr_diff.c — Pure framebuffer diff renderer implementation.
*/

#include "core/zr_diff.h"

#include "util/zr_string_builder.h"

#include <stdbool.h>
#include <string.h>

/* Style attribute bits (v1). */
#define ZR_STYLE_ATTR_BOLD      (1u << 0)
#define ZR_STYLE_ATTR_ITALIC    (1u << 1)
#define ZR_STYLE_ATTR_UNDERLINE (1u << 2)
#define ZR_STYLE_ATTR_REVERSE   (1u << 3)
#define ZR_STYLE_ATTR_STRIKE    (1u << 4)

static bool zr_style_eq(zr_style_t a, zr_style_t b) {
  return a.fg == b.fg && a.bg == b.bg && a.attrs == b.attrs;
}

static bool zr_cell_eq(const zr_fb_cell_t* a, const zr_fb_cell_t* b) {
  if (!a || !b) {
    return false;
  }
  if (a->glyph_len != b->glyph_len) {
    return false;
  }
  if (a->flags != b->flags) {
    return false;
  }
  if (!zr_style_eq(a->style, b->style)) {
    return false;
  }
  if (a->glyph_len != 0u && memcmp(a->glyph, b->glyph, (size_t)a->glyph_len) != 0) {
    return false;
  }
  return true;
}

static bool zr_cell_is_continuation(const zr_fb_cell_t* c) {
  return c && (c->flags & ZR_FB_CELL_FLAG_CONTINUATION) != 0u;
}

static uint8_t zr_cell_width_in_next(const zr_fb_t* fb, uint32_t x, uint32_t y) {
  const zr_fb_cell_t* c = zr_fb_cell_at_const(fb, x, y);
  if (!c) {
    return 1u;
  }
  if (zr_cell_is_continuation(c)) {
    return 0u;
  }
  if (x + 1u < fb->cols) {
    const zr_fb_cell_t* c1 = zr_fb_cell_at_const(fb, x + 1u, y);
    if (zr_cell_is_continuation(c1)) {
      return 2u;
    }
  }
  return 1u;
}

static uint8_t zr_rgb_r(uint32_t rgb) { return (uint8_t)((rgb >> 16) & 0xFFu); }
static uint8_t zr_rgb_g(uint32_t rgb) { return (uint8_t)((rgb >> 8) & 0xFFu); }
static uint8_t zr_rgb_b(uint32_t rgb) { return (uint8_t)(rgb & 0xFFu); }

static uint32_t zr_dist2_u8(uint8_t ar, uint8_t ag, uint8_t ab, uint8_t br, uint8_t bg, uint8_t bb) {
  const int32_t dr = (int32_t)ar - (int32_t)br;
  const int32_t dg = (int32_t)ag - (int32_t)bg;
  const int32_t db = (int32_t)ab - (int32_t)bb;
  return (uint32_t)(dr * dr + dg * dg + db * db);
}

static uint8_t zr_xterm256_component_level(uint8_t v) {
  /* Standard xterm 6x6x6 component levels. */
  static const uint8_t levels[6] = {0u, 95u, 135u, 175u, 215u, 255u};
  uint8_t best_i = 0u;
  uint32_t best_d = 0xFFFFFFFFu;
  for (uint8_t i = 0u; i < 6u; i++) {
    const uint32_t d = (uint32_t)((int32_t)v - (int32_t)levels[i]) * (uint32_t)((int32_t)v - (int32_t)levels[i]);
    if (d < best_d) {
      best_d = d;
      best_i = i;
    }
  }
  return best_i;
}

static uint8_t zr_rgb_to_xterm256(uint32_t rgb) {
  const uint8_t r = zr_rgb_r(rgb);
  const uint8_t g = zr_rgb_g(rgb);
  const uint8_t b = zr_rgb_b(rgb);

  /* Color cube candidate (16..231). */
  static const uint8_t levels[6] = {0u, 95u, 135u, 175u, 215u, 255u};
  const uint8_t ri = zr_xterm256_component_level(r);
  const uint8_t gi = zr_xterm256_component_level(g);
  const uint8_t bi = zr_xterm256_component_level(b);
  const uint8_t cr = levels[ri];
  const uint8_t cg = levels[gi];
  const uint8_t cb = levels[bi];
  const uint8_t cube_idx = (uint8_t)(16u + 36u * ri + 6u * gi + bi);
  const uint32_t cube_d = zr_dist2_u8(r, g, b, cr, cg, cb);

  /* Grayscale ramp candidate (232..255), levels 8 + 10*i (i=0..23). */
  uint8_t best_gray_i = 0u;
  uint32_t best_gray_d = 0xFFFFFFFFu;
  for (uint8_t i = 0u; i < 24u; i++) {
    const uint8_t gv = (uint8_t)(8u + 10u * i);
    const uint32_t d = zr_dist2_u8(r, g, b, gv, gv, gv);
    if (d < best_gray_d) {
      best_gray_d = d;
      best_gray_i = i;
    }
  }
  const uint8_t gray_idx = (uint8_t)(232u + best_gray_i);
  const uint32_t gray_d = best_gray_d;

  if (gray_d < cube_d) {
    return gray_idx;
  }
  if (cube_d < gray_d) {
    return cube_idx;
  }
  /* Tie-break: choose the smaller xterm index deterministically. */
  return (gray_idx < cube_idx) ? gray_idx : cube_idx;
}

static uint8_t zr_rgb_to_ansi16(uint32_t rgb) {
  /* Locked palette (xterm-ish). Indices: 0..15. */
  static const uint8_t pal[16][3] = {
      {0u, 0u, 0u},       {205u, 0u, 0u},     {0u, 205u, 0u},     {205u, 205u, 0u},
      {0u, 0u, 238u},     {205u, 0u, 205u},   {0u, 205u, 205u},   {229u, 229u, 229u},
      {127u, 127u, 127u}, {255u, 0u, 0u},     {0u, 255u, 0u},     {255u, 255u, 0u},
      {92u, 92u, 255u},   {255u, 0u, 255u},   {0u, 255u, 255u},   {255u, 255u, 255u},
  };

  const uint8_t r = zr_rgb_r(rgb);
  const uint8_t g = zr_rgb_g(rgb);
  const uint8_t b = zr_rgb_b(rgb);

  uint8_t best = 0u;
  uint32_t best_d = 0xFFFFFFFFu;
  for (uint8_t i = 0u; i < 16u; i++) {
    const uint32_t d = zr_dist2_u8(r, g, b, pal[i][0], pal[i][1], pal[i][2]);
    if (d < best_d) {
      best_d = d;
      best = i;
    } else if (d == best_d && i < best) {
      best = i;
    }
  }
  return best;
}

static zr_style_t zr_style_apply_caps(zr_style_t in, const plat_caps_t* caps) {
  zr_style_t out = in;
  if (!caps) {
    return out;
  }
  out.attrs &= caps->sgr_attrs_supported;

  if (caps->color_mode == PLAT_COLOR_MODE_RGB) {
    return out;
  }
  if (caps->color_mode == PLAT_COLOR_MODE_256) {
    out.fg = (uint32_t)zr_rgb_to_xterm256(out.fg);
    out.bg = (uint32_t)zr_rgb_to_xterm256(out.bg);
    return out;
  }
  if (caps->color_mode == PLAT_COLOR_MODE_16) {
    out.fg = (uint32_t)zr_rgb_to_ansi16(out.fg);
    out.bg = (uint32_t)zr_rgb_to_ansi16(out.bg);
    return out;
  }

  /* Unknown: deterministically degrade to 16. */
  out.fg = (uint32_t)zr_rgb_to_ansi16(out.fg);
  out.bg = (uint32_t)zr_rgb_to_ansi16(out.bg);
  return out;
}

static bool zr_sb_write_u32_dec(zr_sb_t* sb, uint32_t v) {
  char tmp[10];
  size_t n = 0u;
  do {
    tmp[n++] = (char)('0' + (char)(v % 10u));
    v /= 10u;
  } while (v != 0u && n < sizeof(tmp));

  for (size_t i = 0u; i < n; i++) {
    const uint8_t ch = (uint8_t)tmp[n - 1u - i];
    if (!zr_sb_write_u8(sb, ch)) {
      return false;
    }
  }
  return true;
}

static bool zr_emit_cup(zr_sb_t* sb, zr_term_state_t* ts, uint32_t x, uint32_t y) {
  if (!sb || !ts) {
    return false;
  }
  if (ts->cursor_x == x && ts->cursor_y == y) {
    return true;
  }
  const uint8_t esc = 0x1Bu;
  if (!zr_sb_write_u8(sb, esc) || !zr_sb_write_u8(sb, (uint8_t)'[')) {
    return false;
  }
  if (!zr_sb_write_u32_dec(sb, y + 1u) || !zr_sb_write_u8(sb, (uint8_t)';') ||
      !zr_sb_write_u32_dec(sb, x + 1u) || !zr_sb_write_u8(sb, (uint8_t)'H')) {
    return false;
  }
  ts->cursor_x = x;
  ts->cursor_y = y;
  return true;
}

static bool zr_emit_sgr_absolute(zr_sb_t* sb, zr_term_state_t* ts, zr_style_t desired, const plat_caps_t* caps) {
  if (!sb || !ts) {
    return false;
  }
  desired = zr_style_apply_caps(desired, caps);
  if (zr_style_eq(ts->style, desired)) {
    return true;
  }

  const uint8_t esc = 0x1Bu;
  if (!zr_sb_write_u8(sb, esc) || !zr_sb_write_u8(sb, (uint8_t)'[')) {
    return false;
  }

  /* Always emit a full absolute SGR with reset (v1 deterministic). */
  if (!zr_sb_write_u8(sb, (uint8_t)'0')) {
    return false;
  }

  struct attr_map {
    uint32_t bit;
    uint32_t sgr;
  };
  static const struct attr_map attrs[] = {
      {ZR_STYLE_ATTR_BOLD, 1u},
      {ZR_STYLE_ATTR_ITALIC, 3u},
      {ZR_STYLE_ATTR_UNDERLINE, 4u},
      {ZR_STYLE_ATTR_REVERSE, 7u},
      {ZR_STYLE_ATTR_STRIKE, 9u},
  };

  for (size_t i = 0u; i < (sizeof(attrs) / sizeof(attrs[0])); i++) {
    if ((desired.attrs & attrs[i].bit) != 0u) {
      if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, attrs[i].sgr)) {
        return false;
      }
    }
  }

  if (caps && caps->color_mode == PLAT_COLOR_MODE_RGB) {
    const uint8_t fr = zr_rgb_r(desired.fg);
    const uint8_t fg = zr_rgb_g(desired.fg);
    const uint8_t fb = zr_rgb_b(desired.fg);
    const uint8_t br = zr_rgb_r(desired.bg);
    const uint8_t bg = zr_rgb_g(desired.bg);
    const uint8_t bb = zr_rgb_b(desired.bg);

    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, 38u) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, 2u) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)fr) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)fg) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)fb)) {
      return false;
    }

    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, 48u) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, 2u) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)br) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)bg) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)bb)) {
      return false;
    }
  } else if (caps && caps->color_mode == PLAT_COLOR_MODE_256) {
    const uint32_t fg_idx = desired.fg & 0xFFu;
    const uint32_t bg_idx = desired.bg & 0xFFu;
    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, 38u) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, 5u) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, fg_idx)) {
      return false;
    }
    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, 48u) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, 5u) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, bg_idx)) {
      return false;
    }
  } else {
    /* 16-color (or unknown degraded to 16): desired.fg/bg are indices 0..15. */
    const uint8_t fg_idx = (uint8_t)(desired.fg & 0x0Fu);
    const uint8_t bg_idx = (uint8_t)(desired.bg & 0x0Fu);
    const uint32_t fg_code = (fg_idx < 8u) ? (30u + (uint32_t)fg_idx) : (90u + (uint32_t)(fg_idx - 8u));
    const uint32_t bg_code = (bg_idx < 8u) ? (40u + (uint32_t)bg_idx) : (100u + (uint32_t)(bg_idx - 8u));

    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, fg_code) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, bg_code)) {
      return false;
    }
  }

  if (!zr_sb_write_u8(sb, (uint8_t)'m')) {
    return false;
  }

  ts->style = desired;
  return true;
}

static bool zr_line_dirty_at(const zr_fb_t* prev, const zr_fb_t* next, uint32_t x, uint32_t y) {
  const zr_fb_cell_t* a = zr_fb_cell_at_const(prev, x, y);
  const zr_fb_cell_t* b = zr_fb_cell_at_const(next, x, y);
  if (!a || !b) {
    return false;
  }
  if (!zr_cell_eq(a, b)) {
    return true;
  }
  /* Wide-glyph rule: a dirty continuation forces inclusion of its lead cell. */
  if (x + 1u < prev->cols) {
    const zr_fb_cell_t* a1 = zr_fb_cell_at_const(prev, x + 1u, y);
    const zr_fb_cell_t* b1 = zr_fb_cell_at_const(next, x + 1u, y);
    const bool cont = zr_cell_is_continuation(a1) || zr_cell_is_continuation(b1);
    if (cont && a1 && b1 && !zr_cell_eq(a1, b1)) {
      return true;
    }
  }
  return false;
}

zr_result_t zr_diff_render(const zr_fb_t* prev,
                           const zr_fb_t* next,
                           const plat_caps_t* caps,
                           const zr_term_state_t* initial_term_state,
                           uint8_t* out_buf,
                           size_t out_cap,
                           size_t* out_len,
                           zr_term_state_t* out_final_term_state,
                           zr_diff_stats_t* out_stats) {
  if (out_len) {
    *out_len = 0u;
  }
  if (out_final_term_state) {
    memset(out_final_term_state, 0, sizeof(*out_final_term_state));
  }
  if (out_stats) {
    memset(out_stats, 0, sizeof(*out_stats));
  }

  if (!prev || !next || !caps || !initial_term_state || !out_buf || !out_len || !out_final_term_state ||
      !out_stats) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (prev->cols != next->cols || prev->rows != next->rows) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_sb_t sb;
  zr_sb_init(&sb, out_buf, out_cap);

  zr_term_state_t ts = *initial_term_state;
  zr_diff_stats_t stats = {0u, 0u, 0u};

  for (uint32_t y = 0u; y < next->rows; y++) {
    bool line_dirty = false;

    uint32_t x = 0u;
    while (x < next->cols) {
      if (!zr_line_dirty_at(prev, next, x, y)) {
        x++;
        continue;
      }

      const uint32_t start = x;
      while (x < next->cols && zr_line_dirty_at(prev, next, x, y)) {
        x++;
      }
      const uint32_t end = (x == 0u) ? 0u : (x - 1u);

      if (!zr_emit_cup(&sb, &ts, start, y)) {
        break;
      }

      for (uint32_t xx = start; xx <= end; xx++) {
        const zr_fb_cell_t* c = zr_fb_cell_at_const(next, xx, y);
        if (!c) {
          continue;
        }
        const uint8_t w = zr_cell_width_in_next(next, xx, y);
        if (w == 0u) {
          continue;
        }

        /* If the cursor drifted (e.g. due to skipped continuations), use CUP only. */
        if (!zr_emit_cup(&sb, &ts, xx, y)) {
          break;
        }
        if (!zr_emit_sgr_absolute(&sb, &ts, c->style, caps)) {
          break;
        }
        if (c->glyph_len != 0u) {
          if (!zr_sb_write_bytes(&sb, c->glyph, (size_t)c->glyph_len)) {
            break;
          }
        }

        ts.cursor_x += (uint32_t)w;
      }

      line_dirty = true;
      stats.dirty_cells += (end - start + 1u);

      if (zr_sb_truncated(&sb)) {
        break;
      }
    }

    if (line_dirty) {
      stats.dirty_lines++;
    }

    if (zr_sb_truncated(&sb)) {
      break;
    }
  }

  if (zr_sb_truncated(&sb)) {
    *out_len = 0u;
    memset(out_final_term_state, 0, sizeof(*out_final_term_state));
    memset(out_stats, 0, sizeof(*out_stats));
    return ZR_ERR_LIMIT;
  }

  *out_len = zr_sb_len(&sb);
  *out_final_term_state = ts;
  stats.bytes_emitted = *out_len;
  *out_stats = stats;
  return ZR_OK;
}

