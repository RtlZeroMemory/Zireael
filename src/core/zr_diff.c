/*
  src/core/zr_diff.c — Pure framebuffer diff renderer implementation.
*/

#include "core/zr_diff.h"

#include "util/zr_string_builder.h"

#include <stdbool.h>
#include <string.h>

/* --- Color Format Constants --- */

/* RGB color format: 0x00RRGGBB (red in bits 16-23, green 8-15, blue 0-7). */
#define ZR_RGB_R_SHIFT 16u
#define ZR_RGB_G_SHIFT 8u
#define ZR_RGB_MASK    0xFFu

/* xterm 256-color cube: 6 levels per channel (indices 16-231). */
static const uint8_t ZR_XTERM256_LEVELS[6] = {0u, 95u, 135u, 175u, 215u, 255u};
#define ZR_XTERM256_CUBE_START 16u
#define ZR_XTERM256_CUBE_SIZE  6u

/* xterm 256-color grayscale ramp: 24 shades (indices 232-255). */
#define ZR_XTERM256_GRAY_START 232u
#define ZR_XTERM256_GRAY_COUNT 24u
#define ZR_XTERM256_GRAY_BASE  8u  /* First gray level value */
#define ZR_XTERM256_GRAY_STEP  10u /* Increment per gray level */

/* xterm-compatible 16-color palette (ANSI colors 0-15). */
static const uint8_t ZR_ANSI16_PALETTE[16][3] = {
    /* Standard colors (0-7) */
    {0u, 0u, 0u},       /* 0: Black */
    {205u, 0u, 0u},     /* 1: Red */
    {0u, 205u, 0u},     /* 2: Green */
    {205u, 205u, 0u},   /* 3: Yellow */
    {0u, 0u, 238u},     /* 4: Blue */
    {205u, 0u, 205u},   /* 5: Magenta */
    {0u, 205u, 205u},   /* 6: Cyan */
    {229u, 229u, 229u}, /* 7: White */
    /* Bright colors (8-15) */
    {127u, 127u, 127u}, /* 8: Bright Black (Gray) */
    {255u, 0u, 0u},     /* 9: Bright Red */
    {0u, 255u, 0u},     /* 10: Bright Green */
    {255u, 255u, 0u},   /* 11: Bright Yellow */
    {92u, 92u, 255u},   /* 12: Bright Blue */
    {255u, 0u, 255u},   /* 13: Bright Magenta */
    {0u, 255u, 255u},   /* 14: Bright Cyan */
    {255u, 255u, 255u}, /* 15: Bright White */
};

/* SGR (Select Graphic Rendition) codes. */
#define ZR_SGR_RESET          0u
#define ZR_SGR_BOLD           1u
#define ZR_SGR_ITALIC         3u
#define ZR_SGR_UNDERLINE      4u
#define ZR_SGR_REVERSE        7u
#define ZR_SGR_STRIKETHROUGH  9u
#define ZR_SGR_FG_256         38u /* Extended foreground color */
#define ZR_SGR_BG_256         48u /* Extended background color */
#define ZR_SGR_COLOR_MODE_256 5u  /* 256-color mode selector */
#define ZR_SGR_COLOR_MODE_RGB 2u  /* RGB color mode selector */

/* ANSI 16-color SGR base codes. */
#define ZR_SGR_FG_BASE   30u  /* FG colors 0-7: 30-37 */
#define ZR_SGR_FG_BRIGHT 90u  /* FG colors 8-15: 90-97 */
#define ZR_SGR_BG_BASE   40u  /* BG colors 0-7: 40-47 */
#define ZR_SGR_BG_BRIGHT 100u /* BG colors 8-15: 100-107 */

/* Style attribute bits (v1). */
#define ZR_STYLE_ATTR_BOLD      (1u << 0)
#define ZR_STYLE_ATTR_ITALIC    (1u << 1)
#define ZR_STYLE_ATTR_UNDERLINE (1u << 2)
#define ZR_STYLE_ATTR_REVERSE   (1u << 3)
#define ZR_STYLE_ATTR_STRIKE    (1u << 4)

static bool zr_style_eq(zr_style_t a, zr_style_t b) {
  return a.fg == b.fg && a.bg == b.bg && a.attrs == b.attrs;
}

/* Compare two framebuffer cells for equality (glyph, flags, and style). */
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

/* Return display width of cell at (x,y): 0 for continuation, 2 for wide, 1 otherwise. */
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

static uint8_t zr_rgb_r(uint32_t rgb) { return (uint8_t)((rgb >> ZR_RGB_R_SHIFT) & ZR_RGB_MASK); }
static uint8_t zr_rgb_g(uint32_t rgb) { return (uint8_t)((rgb >> ZR_RGB_G_SHIFT) & ZR_RGB_MASK); }
static uint8_t zr_rgb_b(uint32_t rgb) { return (uint8_t)(rgb & ZR_RGB_MASK); }

/* Compute squared Euclidean distance between two RGB colors. */
static uint32_t zr_dist2_u8(uint8_t ar, uint8_t ag, uint8_t ab, uint8_t br, uint8_t bg, uint8_t bb) {
  const int32_t dr = (int32_t)ar - (int32_t)br;
  const int32_t dg = (int32_t)ag - (int32_t)bg;
  const int32_t db = (int32_t)ab - (int32_t)bb;
  return (uint32_t)(dr * dr + dg * dg + db * db);
}

/* Find the nearest xterm 256-color cube level (0-5) for a single RGB component. */
static uint8_t zr_xterm256_component_level(uint8_t v) {
  uint8_t best_i = 0u;
  uint32_t best_d = 0xFFFFFFFFu;
  for (uint8_t i = 0u; i < ZR_XTERM256_CUBE_SIZE; i++) {
    const int32_t diff = (int32_t)v - (int32_t)ZR_XTERM256_LEVELS[i];
    const uint32_t d = (uint32_t)(diff * diff);
    if (d < best_d) {
      best_d = d;
      best_i = i;
    }
  }
  return best_i;
}

/*
 * Map 24-bit RGB to nearest xterm 256-color index.
 * Compares against both the 6x6x6 color cube (16-231) and
 * grayscale ramp (232-255), returning whichever is closer.
 */
static uint8_t zr_rgb_to_xterm256(uint32_t rgb) {
  const uint8_t r = zr_rgb_r(rgb);
  const uint8_t g = zr_rgb_g(rgb);
  const uint8_t b = zr_rgb_b(rgb);

  /* Color cube candidate (16..231). */
  const uint8_t ri = zr_xterm256_component_level(r);
  const uint8_t gi = zr_xterm256_component_level(g);
  const uint8_t bi = zr_xterm256_component_level(b);
  const uint8_t cr = ZR_XTERM256_LEVELS[ri];
  const uint8_t cg = ZR_XTERM256_LEVELS[gi];
  const uint8_t cb = ZR_XTERM256_LEVELS[bi];
  const uint8_t cube_idx = (uint8_t)(ZR_XTERM256_CUBE_START +
                                     (ZR_XTERM256_CUBE_SIZE * ZR_XTERM256_CUBE_SIZE) * ri +
                                     ZR_XTERM256_CUBE_SIZE * gi + bi);
  const uint32_t cube_d = zr_dist2_u8(r, g, b, cr, cg, cb);

  /* Grayscale ramp candidate (232..255), levels 8 + 10*i (i=0..23). */
  uint8_t best_gray_i = 0u;
  uint32_t best_gray_d = 0xFFFFFFFFu;
  for (uint8_t i = 0u; i < ZR_XTERM256_GRAY_COUNT; i++) {
    const uint8_t gv = (uint8_t)(ZR_XTERM256_GRAY_BASE + ZR_XTERM256_GRAY_STEP * i);
    const uint32_t d = zr_dist2_u8(r, g, b, gv, gv, gv);
    if (d < best_gray_d) {
      best_gray_d = d;
      best_gray_i = i;
    }
  }
  const uint8_t gray_idx = (uint8_t)(ZR_XTERM256_GRAY_START + best_gray_i);
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

/* Map 24-bit RGB to nearest ANSI 16-color index (0-15). */
static uint8_t zr_rgb_to_ansi16(uint32_t rgb) {
  const uint8_t r = zr_rgb_r(rgb);
  const uint8_t g = zr_rgb_g(rgb);
  const uint8_t b = zr_rgb_b(rgb);

  uint8_t best = 0u;
  uint32_t best_d = 0xFFFFFFFFu;
  for (uint8_t i = 0u; i < 16u; i++) {
    const uint32_t d = zr_dist2_u8(r, g, b, ZR_ANSI16_PALETTE[i][0], ZR_ANSI16_PALETTE[i][1],
                                   ZR_ANSI16_PALETTE[i][2]);
    if (d < best_d) {
      best_d = d;
      best = i;
    } else if (d == best_d && i < best) {
      best = i;
    }
  }
  return best;
}

/* Downgrade style colors/attrs based on terminal capabilities (RGB → 256 → 16). */
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

/* Write u32 as decimal ASCII digits to string builder. */
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

/* Emit CUP (cursor position) escape sequence if cursor is not already at (x,y). */
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

/* Emit full SGR sequence (reset + attrs + colors) if style differs from current. */
static bool zr_emit_sgr_absolute(zr_sb_t* sb, zr_term_state_t* ts, zr_style_t desired, const plat_caps_t* caps) {
  if (!sb || !ts) {
    return false;
  }
  desired = zr_style_apply_caps(desired, caps);
  if (zr_style_eq(ts->style, desired)) {
    return true;
  }

  const uint8_t esc = 0x1Bu;

  /* --- Begin SGR sequence with reset --- */
  if (!zr_sb_write_u8(sb, esc) || !zr_sb_write_u8(sb, (uint8_t)'[')) {
    return false;
  }

  /* Always emit a full absolute SGR with reset (v1 deterministic). */
  if (!zr_sb_write_u32_dec(sb, ZR_SGR_RESET)) {
    return false;
  }

  struct attr_map {
    uint32_t bit;
    uint32_t sgr;
  };
  static const struct attr_map attrs[] = {
      {ZR_STYLE_ATTR_BOLD, ZR_SGR_BOLD},
      {ZR_STYLE_ATTR_ITALIC, ZR_SGR_ITALIC},
      {ZR_STYLE_ATTR_UNDERLINE, ZR_SGR_UNDERLINE},
      {ZR_STYLE_ATTR_REVERSE, ZR_SGR_REVERSE},
      {ZR_STYLE_ATTR_STRIKE, ZR_SGR_STRIKETHROUGH},
  };

  /* --- Text attributes (bold, italic, etc.) --- */
  for (size_t i = 0u; i < (sizeof(attrs) / sizeof(attrs[0])); i++) {
    if ((desired.attrs & attrs[i].bit) != 0u) {
      if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, attrs[i].sgr)) {
        return false;
      }
    }
  }

  /* --- Foreground and background colors --- */
  if (caps && caps->color_mode == PLAT_COLOR_MODE_RGB) {
    const uint8_t fr = zr_rgb_r(desired.fg);
    const uint8_t fg = zr_rgb_g(desired.fg);
    const uint8_t fb = zr_rgb_b(desired.fg);
    const uint8_t br = zr_rgb_r(desired.bg);
    const uint8_t bg = zr_rgb_g(desired.bg);
    const uint8_t bb = zr_rgb_b(desired.bg);

    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, ZR_SGR_FG_256) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, ZR_SGR_COLOR_MODE_RGB) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)fr) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)fg) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)fb)) {
      return false;
    }

    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, ZR_SGR_BG_256) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, ZR_SGR_COLOR_MODE_RGB) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)br) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)bg) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, (uint32_t)bb)) {
      return false;
    }
  } else if (caps && caps->color_mode == PLAT_COLOR_MODE_256) {
    const uint32_t fg_idx = desired.fg & 0xFFu;
    const uint32_t bg_idx = desired.bg & 0xFFu;
    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, ZR_SGR_FG_256) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, ZR_SGR_COLOR_MODE_256) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, fg_idx)) {
      return false;
    }
    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, ZR_SGR_BG_256) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, ZR_SGR_COLOR_MODE_256) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, bg_idx)) {
      return false;
    }
  } else {
    /* 16-color (or unknown degraded to 16): desired.fg/bg are indices 0..15. */
    const uint8_t fg_idx = (uint8_t)(desired.fg & 0x0Fu);
    const uint8_t bg_idx = (uint8_t)(desired.bg & 0x0Fu);
    const uint32_t fg_code =
        (fg_idx < 8u) ? (ZR_SGR_FG_BASE + (uint32_t)fg_idx) : (ZR_SGR_FG_BRIGHT + (uint32_t)(fg_idx - 8u));
    const uint32_t bg_code =
        (bg_idx < 8u) ? (ZR_SGR_BG_BASE + (uint32_t)bg_idx) : (ZR_SGR_BG_BRIGHT + (uint32_t)(bg_idx - 8u));

    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, fg_code) ||
        !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, bg_code)) {
      return false;
    }
  }

  /* --- Finalize SGR sequence --- */
  if (!zr_sb_write_u8(sb, (uint8_t)'m')) {
    return false;
  }

  ts->style = desired;
  return true;
}

/* Check if cell at (x,y) differs between prev and next framebuffers.
 * Also returns true if wide-glyph continuation cell changed. */
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

typedef struct zr_diff_ctx_t {
  const zr_fb_t* prev;
  const zr_fb_t* next;
  const plat_caps_t* caps;
  zr_sb_t sb;
  zr_term_state_t ts;
  zr_diff_stats_t stats;
} zr_diff_ctx_t;

static void zr_diff_zero_outputs(size_t* out_len, zr_term_state_t* out_final_term_state, zr_diff_stats_t* out_stats) {
  if (out_len) {
    *out_len = 0u;
  }
  if (out_final_term_state) {
    memset(out_final_term_state, 0, sizeof(*out_final_term_state));
  }
  if (out_stats) {
    memset(out_stats, 0, sizeof(*out_stats));
  }
}

static zr_result_t zr_diff_validate_args(const zr_fb_t* prev,
                                        const zr_fb_t* next,
                                        const plat_caps_t* caps,
                                        const zr_term_state_t* initial_term_state,
                                        const uint8_t* out_buf,
                                        const size_t* out_len,
                                        const zr_term_state_t* out_final_term_state,
                                        const zr_diff_stats_t* out_stats) {
  if (!prev || !next || !caps || !initial_term_state || !out_buf || !out_len || !out_final_term_state || !out_stats) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (prev->cols != next->cols || prev->rows != next->rows) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return ZR_OK;
}

/* Render a contiguous span of dirty cells [start, end] on row y. */
static zr_result_t zr_diff_render_span(zr_diff_ctx_t* ctx, uint32_t y, uint32_t start, uint32_t end) {
  if (!ctx || !ctx->prev || !ctx->next) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!zr_emit_cup(&ctx->sb, &ctx->ts, start, y)) {
    return ZR_ERR_LIMIT;
  }

  for (uint32_t xx = start; xx <= end; xx++) {
    const zr_fb_cell_t* c = zr_fb_cell_at_const(ctx->next, xx, y);
    if (!c) {
      continue;
    }
    const uint8_t w = zr_cell_width_in_next(ctx->next, xx, y);
    if (w == 0u) {
      continue;
    }

    /* If the cursor drifted (e.g. due to skipped continuations), use CUP only. */
    if (!zr_emit_cup(&ctx->sb, &ctx->ts, xx, y)) {
      return ZR_ERR_LIMIT;
    }
    if (!zr_emit_sgr_absolute(&ctx->sb, &ctx->ts, c->style, ctx->caps)) {
      return ZR_ERR_LIMIT;
    }
    if (c->glyph_len != 0u) {
      if (!zr_sb_write_bytes(&ctx->sb, c->glyph, (size_t)c->glyph_len)) {
        return ZR_ERR_LIMIT;
      }
    }

    ctx->ts.cursor_x += (uint32_t)w;
  }

  return zr_sb_truncated(&ctx->sb) ? ZR_ERR_LIMIT : ZR_OK;
}

/* Scan row y for dirty spans and render each one. */
static zr_result_t zr_diff_render_line(zr_diff_ctx_t* ctx, uint32_t y) {
  if (!ctx || !ctx->prev || !ctx->next) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  bool line_dirty = false;

  uint32_t x = 0u;
  while (x < ctx->next->cols) {
    if (!zr_line_dirty_at(ctx->prev, ctx->next, x, y)) {
      x++;
      continue;
    }

    const uint32_t start = x;
    while (x < ctx->next->cols && zr_line_dirty_at(ctx->prev, ctx->next, x, y)) {
      x++;
    }
    const uint32_t end = (x == 0u) ? 0u : (x - 1u);

    const zr_result_t rc = zr_diff_render_span(ctx, y, start, end);
    if (rc != ZR_OK) {
      return rc;
    }

    line_dirty = true;
    ctx->stats.dirty_cells += (end - start + 1u);

    if (zr_sb_truncated(&ctx->sb)) {
      return ZR_ERR_LIMIT;
    }
  }

  if (line_dirty) {
    ctx->stats.dirty_lines++;
  }
  return ZR_OK;
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
  /*
   * Render the difference between two framebuffers as VT/ANSI escape sequences.
   *
   * Iterates row-by-row, emitting cursor positioning (CUP) and styling (SGR)
   * only for cells that changed between prev and next. Wide characters are
   * handled by checking continuation flags.
   *
   * On success: writes output to out_buf, updates out_len/out_final_term_state/out_stats.
   * On failure: zeros all outputs and returns error code (no partial writes).
   */
  zr_diff_zero_outputs(out_len, out_final_term_state, out_stats);

  const zr_result_t arg_rc =
      zr_diff_validate_args(prev, next, caps, initial_term_state, out_buf, out_len, out_final_term_state, out_stats);
  if (arg_rc != ZR_OK) {
    return arg_rc;
  }

  zr_diff_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.prev = prev;
  ctx.next = next;
  ctx.caps = caps;
  zr_sb_init(&ctx.sb, out_buf, out_cap);
  ctx.ts = *initial_term_state;

  for (uint32_t y = 0u; y < next->rows; y++) {
    const zr_result_t rc = zr_diff_render_line(&ctx, y);
    if (rc != ZR_OK) {
      zr_diff_zero_outputs(out_len, out_final_term_state, out_stats);
      return rc;
    }
  }

  if (zr_sb_truncated(&ctx.sb)) {
    zr_diff_zero_outputs(out_len, out_final_term_state, out_stats);
    return ZR_ERR_LIMIT;
  }

  *out_len = zr_sb_len(&ctx.sb);
  *out_final_term_state = ctx.ts;
  ctx.stats.bytes_emitted = *out_len;
  *out_stats = ctx.stats;
  return ZR_OK;
}
