/*
  src/core/zr_diff.c — Pure framebuffer diff renderer implementation.

  Why: Emits minimal VT output by diffing previous/next framebuffers while
  preserving grapheme/style correctness and deterministic terminal state.
*/

#include "core/zr_diff.h"

#include "util/zr_checked.h"
#include "util/zr_string_builder.h"

#include <stdbool.h>
#include <string.h>

/* --- Color Format Constants --- */

/* RGB color format: 0x00RRGGBB (red in bits 16-23, green 8-15, blue 0-7). */
#define ZR_RGB_R_SHIFT 16u
#define ZR_RGB_G_SHIFT 8u
#define ZR_RGB_MASK 0xFFu

/* xterm 256-color cube: 6 levels per channel (indices 16-231). */
static const uint8_t ZR_XTERM256_LEVELS[6] = {0u, 95u, 135u, 175u, 215u, 255u};
#define ZR_XTERM256_CUBE_START 16u
#define ZR_XTERM256_CUBE_SIZE 6u

/* xterm 256-color grayscale ramp: 24 shades (indices 232-255). */
#define ZR_XTERM256_GRAY_START 232u
#define ZR_XTERM256_GRAY_COUNT 24u
#define ZR_XTERM256_GRAY_BASE 8u  /* First gray level value */
#define ZR_XTERM256_GRAY_STEP 10u /* Increment per gray level */

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
#define ZR_SGR_RESET 0u
#define ZR_SGR_BOLD 1u
#define ZR_SGR_ITALIC 3u
#define ZR_SGR_UNDERLINE 4u
#define ZR_SGR_REVERSE 7u
#define ZR_SGR_STRIKETHROUGH 9u
#define ZR_SGR_FG_256 38u        /* Extended foreground color */
#define ZR_SGR_BG_256 48u        /* Extended background color */
#define ZR_SGR_COLOR_MODE_256 5u /* 256-color mode selector */
#define ZR_SGR_COLOR_MODE_RGB 2u /* RGB color mode selector */

/* ANSI 16-color SGR base codes. */
#define ZR_SGR_FG_BASE 30u    /* FG colors 0-7: 30-37 */
#define ZR_SGR_FG_BRIGHT 90u  /* FG colors 8-15: 90-97 */
#define ZR_SGR_BG_BASE 40u    /* BG colors 0-7: 40-47 */
#define ZR_SGR_BG_BRIGHT 100u /* BG colors 8-15: 100-107 */

/* Style attribute bits (v1). */
#define ZR_STYLE_ATTR_BOLD (1u << 0)
#define ZR_STYLE_ATTR_ITALIC (1u << 1)
#define ZR_STYLE_ATTR_UNDERLINE (1u << 2)
#define ZR_STYLE_ATTR_REVERSE (1u << 3)
#define ZR_STYLE_ATTR_STRIKE (1u << 4)

static bool zr_style_eq(zr_style_t a, zr_style_t b) {
  return a.fg_rgb == b.fg_rgb && a.bg_rgb == b.bg_rgb && a.attrs == b.attrs && a.reserved == b.reserved;
}

/* Compare two framebuffer cells for equality (glyph, flags, and style). */
static bool zr_cell_eq(const zr_cell_t* a, const zr_cell_t* b) {
  if (!a || !b) {
    return false;
  }
  if (a->glyph_len != b->glyph_len) {
    return false;
  }
  if (a->width != b->width) {
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

static bool zr_cell_is_continuation(const zr_cell_t* c) {
  return c && c->width == 0u;
}

/* Return display width of cell at (x,y): 0 for continuation, 2 for wide, 1 otherwise. */
static uint8_t zr_cell_width_in_next(const zr_fb_t* fb, uint32_t x, uint32_t y) {
  const zr_cell_t* c = zr_fb_cell_const(fb, x, y);
  if (!c) {
    return 1u;
  }
  if (zr_cell_is_continuation(c)) {
    return 0u;
  }
  if (c->width == 2u) {
    return 2u;
  }
  if (x + 1u < fb->cols) {
    const zr_cell_t* c1 = zr_fb_cell_const(fb, x + 1u, y);
    if (zr_cell_is_continuation(c1)) {
      return 2u;
    }
  }
  return 1u;
}

static uint8_t zr_rgb_r(uint32_t rgb) {
  return (uint8_t)((rgb >> ZR_RGB_R_SHIFT) & ZR_RGB_MASK);
}
static uint8_t zr_rgb_g(uint32_t rgb) {
  return (uint8_t)((rgb >> ZR_RGB_G_SHIFT) & ZR_RGB_MASK);
}
static uint8_t zr_rgb_b(uint32_t rgb) {
  return (uint8_t)(rgb & ZR_RGB_MASK);
}

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
  const uint8_t cube_idx = (uint8_t)(ZR_XTERM256_CUBE_START + (ZR_XTERM256_CUBE_SIZE * ZR_XTERM256_CUBE_SIZE) * ri +
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
    const uint32_t d = zr_dist2_u8(r, g, b, ZR_ANSI16_PALETTE[i][0], ZR_ANSI16_PALETTE[i][1], ZR_ANSI16_PALETTE[i][2]);
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
    out.fg_rgb = (uint32_t)zr_rgb_to_xterm256(out.fg_rgb);
    out.bg_rgb = (uint32_t)zr_rgb_to_xterm256(out.bg_rgb);
    return out;
  }
  if (caps->color_mode == PLAT_COLOR_MODE_16) {
    out.fg_rgb = (uint32_t)zr_rgb_to_ansi16(out.fg_rgb);
    out.bg_rgb = (uint32_t)zr_rgb_to_ansi16(out.bg_rgb);
    return out;
  }

  /* Unknown: deterministically degrade to 16. */
  out.fg_rgb = (uint32_t)zr_rgb_to_ansi16(out.fg_rgb);
  out.bg_rgb = (uint32_t)zr_rgb_to_ansi16(out.bg_rgb);
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
  if (!zr_sb_write_u32_dec(sb, y + 1u) || !zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, x + 1u) ||
      !zr_sb_write_u8(sb, (uint8_t)'H')) {
    return false;
  }
  ts->cursor_x = x;
  ts->cursor_y = y;
  return true;
}

static bool zr_emit_cursor_visibility(zr_sb_t* sb, zr_term_state_t* ts, uint8_t visible) {
  if (!sb || !ts) {
    return false;
  }
  if (visible > 1u) {
    return false;
  }
  if (ts->cursor_visible == visible) {
    return true;
  }

  const uint8_t seq_show[] = "\x1b[?25h";
  const uint8_t seq_hide[] = "\x1b[?25l";
  const uint8_t* seq = (visible != 0u) ? seq_show : seq_hide;
  const size_t n = sizeof(seq_show) - 1u;
  if (!zr_sb_write_bytes(sb, seq, n)) {
    return false;
  }
  ts->cursor_visible = visible;
  return true;
}

static uint32_t zr_cursor_shape_ps(uint8_t shape, uint8_t blink) {
  if (shape == ZR_CURSOR_SHAPE_UNDERLINE) {
    return (blink != 0u) ? 3u : 4u;
  }
  if (shape == ZR_CURSOR_SHAPE_BAR) {
    return (blink != 0u) ? 5u : 6u;
  }
  return (blink != 0u) ? 1u : 2u;
}

static bool zr_emit_cursor_shape(zr_sb_t* sb, zr_term_state_t* ts, uint8_t shape, uint8_t blink,
                                 const plat_caps_t* caps) {
  if (!sb || !ts || !caps) {
    return false;
  }
  if (shape > ZR_CURSOR_SHAPE_BAR || blink > 1u) {
    return false;
  }
  if (caps->supports_cursor_shape == 0u) {
    return true;
  }
  if (ts->cursor_shape == shape && ts->cursor_blink == blink) {
    return true;
  }

  const uint32_t ps = zr_cursor_shape_ps(shape, blink);
  if (!zr_sb_write_u8(sb, 0x1Bu) || !zr_sb_write_u8(sb, (uint8_t)'[') || !zr_sb_write_u32_dec(sb, ps) ||
      !zr_sb_write_u8(sb, (uint8_t)' ') || !zr_sb_write_u8(sb, (uint8_t)'q')) {
    return false;
  }

  ts->cursor_shape = shape;
  ts->cursor_blink = blink;
  return true;
}

static uint32_t zr_clamp_u32_from_i32(int32_t v, uint32_t lo, uint32_t hi) {
  if (hi < lo) {
    return lo;
  }
  if (v <= (int32_t)lo) {
    return lo;
  }
  if (v >= (int32_t)hi) {
    return hi;
  }
  return (uint32_t)v;
}

static bool zr_emit_cursor_desired(zr_sb_t* sb, zr_term_state_t* ts, const zr_cursor_state_t* desired,
                                   const zr_fb_t* next, const plat_caps_t* caps) {
  if (!sb || !ts || !next || !caps) {
    return false;
  }
  if (!desired) {
    return true;
  }

  if (!zr_emit_cursor_shape(sb, ts, desired->shape, desired->blink, caps)) {
    return false;
  }
  if (!zr_emit_cursor_visibility(sb, ts, desired->visible)) {
    return false;
  }

  if (next->cols == 0u || next->rows == 0u) {
    return true;
  }

  uint32_t x = ts->cursor_x;
  uint32_t y = ts->cursor_y;
  if (desired->x != -1) {
    x = zr_clamp_u32_from_i32(desired->x, 0u, next->cols - 1u);
  }
  if (desired->y != -1) {
    y = zr_clamp_u32_from_i32(desired->y, 0u, next->rows - 1u);
  }

  return zr_emit_cup(sb, ts, x, y);
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
    const uint8_t fr = zr_rgb_r(desired.fg_rgb);
    const uint8_t fg = zr_rgb_g(desired.fg_rgb);
    const uint8_t fb = zr_rgb_b(desired.fg_rgb);
    const uint8_t br = zr_rgb_r(desired.bg_rgb);
    const uint8_t bg = zr_rgb_g(desired.bg_rgb);
    const uint8_t bb = zr_rgb_b(desired.bg_rgb);

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
    const uint32_t fg_idx = desired.fg_rgb & 0xFFu;
    const uint32_t bg_idx = desired.bg_rgb & 0xFFu;
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
    /* 16-color (or unknown degraded to 16): desired.fg_rgb/bg_rgb are indices 0..15. */
    const uint8_t fg_idx = (uint8_t)(desired.fg_rgb & 0x0Fu);
    const uint8_t bg_idx = (uint8_t)(desired.bg_rgb & 0x0Fu);
    const uint32_t fg_code =
        (fg_idx < 8u) ? (ZR_SGR_FG_BASE + (uint32_t)fg_idx) : (ZR_SGR_FG_BRIGHT + (uint32_t)(fg_idx - 8u));
    const uint32_t bg_code =
        (bg_idx < 8u) ? (ZR_SGR_BG_BASE + (uint32_t)bg_idx) : (ZR_SGR_BG_BRIGHT + (uint32_t)(bg_idx - 8u));

    if (!zr_sb_write_u8(sb, (uint8_t)';') || !zr_sb_write_u32_dec(sb, fg_code) || !zr_sb_write_u8(sb, (uint8_t)';') ||
        !zr_sb_write_u32_dec(sb, bg_code)) {
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
  const zr_cell_t* a = zr_fb_cell_const(prev, x, y);
  const zr_cell_t* b = zr_fb_cell_const(next, x, y);
  if (!a || !b) {
    return false;
  }
  if (!zr_cell_eq(a, b)) {
    return true;
  }
  /* Wide-glyph rule: a dirty continuation forces inclusion of its lead cell. */
  if (x + 1u < prev->cols) {
    const zr_cell_t* a1 = zr_fb_cell_const(prev, x + 1u, y);
    const zr_cell_t* b1 = zr_fb_cell_const(next, x + 1u, y);
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
  zr_damage_t damage;
} zr_diff_ctx_t;

typedef struct zr_scroll_plan_t {
  bool active;
  bool up;
  uint32_t top;
  uint32_t bottom;
  uint32_t lines;
  uint32_t moved_lines;
} zr_scroll_plan_t;

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

static zr_result_t zr_diff_validate_args(const zr_fb_t* prev, const zr_fb_t* next, const plat_caps_t* caps,
                                         const zr_term_state_t* initial_term_state,
                                         const zr_cursor_state_t* desired_cursor_state, const zr_limits_t* lim,
                                         zr_damage_rect_t* scratch_damage_rects, uint32_t scratch_damage_rect_cap,
                                         uint8_t enable_scroll_optimizations, const uint8_t* out_buf,
                                         const size_t* out_len, const zr_term_state_t* out_final_term_state,
                                         const zr_diff_stats_t* out_stats) {
  (void)enable_scroll_optimizations;
  (void)desired_cursor_state;
  if (!prev || !next || !caps || !initial_term_state || !lim || !out_buf || !out_len || !out_final_term_state ||
      !out_stats) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (prev->cols != next->cols || prev->rows != next->rows) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!scratch_damage_rects || scratch_damage_rect_cap < lim->diff_max_damage_rects) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return ZR_OK;
}

/* Compare full framebuffer rows for scroll-shift detection (full width). */
static bool zr_row_eq(const zr_fb_t* a, uint32_t ay, const zr_fb_t* b, uint32_t by) {
  if (!a || !b) {
    return false;
  }
  if (a->cols != b->cols) {
    return false;
  }
  if (ay >= a->rows || by >= b->rows) {
    return false;
  }

  for (uint32_t x = 0u; x < a->cols; x++) {
    const zr_cell_t* ca = zr_fb_cell_const(a, x, ay);
    const zr_cell_t* cb = zr_fb_cell_const(b, x, by);
    if (!ca || !cb) {
      return false;
    }
    if (!zr_cell_eq(ca, cb)) {
      return false;
    }
  }
  return true;
}

/* Deterministic preference order for competing scroll candidates. */
static bool zr_scroll_plan_better(const zr_scroll_plan_t* best, const zr_scroll_plan_t* cand, uint32_t cols) {
  if (!cand || !cand->active) {
    return false;
  }
  if (!best || !best->active) {
    return true;
  }

  const uint64_t best_cells = (uint64_t)best->moved_lines * (uint64_t)cols;
  const uint64_t cand_cells = (uint64_t)cand->moved_lines * (uint64_t)cols;
  if (cand_cells != best_cells) {
    return cand_cells > best_cells;
  }
  if (cand->moved_lines != best->moved_lines) {
    return cand->moved_lines > best->moved_lines;
  }
  if (cand->lines != best->lines) {
    return cand->lines < best->lines;
  }
  if (cand->top != best->top) {
    return cand->top < best->top;
  }
  if (cand->bottom != best->bottom) {
    return cand->bottom < best->bottom;
  }
  if (cand->up != best->up) {
    return cand->up;
  }
  return false;
}

static bool zr_scroll_saved_enough(uint32_t moved_lines, uint32_t cols) {
  enum {
    ZR_SCROLL_MIN_MOVED_LINES = 4u,
    ZR_SCROLL_MIN_SAVED_CELLS = 256u,
  };

  if (moved_lines < ZR_SCROLL_MIN_MOVED_LINES) {
    return false;
  }
  const uint64_t saved_cells = (uint64_t)moved_lines * (uint64_t)cols;
  return saved_cells >= (uint64_t)ZR_SCROLL_MIN_SAVED_CELLS;
}

/* Evaluate a contiguous run of row matches as a scroll-region candidate. */
static void zr_scroll_plan_consider_run(zr_scroll_plan_t* best, uint32_t cols, uint32_t rows, bool up,
                                        uint32_t run_start, uint32_t run_len, uint32_t delta) {
  if (!best || run_len == 0u || delta == 0u) {
    return;
  }

  zr_scroll_plan_t cand;
  memset(&cand, 0, sizeof(cand));
  cand.active = true;
  cand.up = up;
  cand.top = run_start;
  cand.bottom = (run_start + run_len - 1u) + delta;
  cand.lines = delta;
  cand.moved_lines = run_len;

  if (cand.bottom >= rows) {
    return;
  }
  if (!zr_scroll_saved_enough(cand.moved_lines, cols)) {
    return;
  }

  if (zr_scroll_plan_better(best, &cand, cols)) {
    *best = cand;
  }
}

/* Scan for the longest run of shifted-equal rows for a given delta + direction. */
static void zr_scroll_scan_delta_dir(const zr_fb_t* prev, const zr_fb_t* next, uint32_t delta, bool up,
                                     zr_scroll_plan_t* inout_best) {
  if (!prev || !next || !inout_best) {
    return;
  }
  if (delta == 0u || delta >= next->rows) {
    return;
  }

  const uint32_t rows = next->rows;
  const uint32_t cols = next->cols;
  const uint32_t y_end = rows - delta;

  uint32_t run_start = 0u;
  uint32_t run_len = 0u;

  for (uint32_t y = 0u; y < y_end; y++) {
    const bool match = up ? zr_row_eq(next, y, prev, y + delta) : zr_row_eq(next, y + delta, prev, y);
    if (match) {
      if (run_len == 0u) {
        run_start = y;
      }
      run_len++;
      continue;
    }

    zr_scroll_plan_consider_run(inout_best, cols, rows, up, run_start, run_len, delta);
    run_len = 0u;
  }

  zr_scroll_plan_consider_run(inout_best, cols, rows, up, run_start, run_len, delta);
}

/*
 * Detect a vertical scroll within a full-width region.
 *
 * Why: When a large block of rows is identical after a vertical shift, emitting
 * DECSTBM + SU/SD lets the terminal do the bulk move and keeps output bounded
 * to the newly exposed lines.
 */
static zr_scroll_plan_t zr_diff_detect_scroll_fullwidth(const zr_fb_t* prev, const zr_fb_t* next) {
  zr_scroll_plan_t best;
  memset(&best, 0, sizeof(best));

  if (!prev || !next || prev->cols != next->cols || prev->rows != next->rows) {
    return best;
  }
  if (next->rows < 2u || next->cols == 0u) {
    return best;
  }

  const uint32_t rows = next->rows;

  enum {
    ZR_SCROLL_MAX_DELTA = 64u,
  };

  uint32_t max_delta = rows - 1u;
  if (max_delta > ZR_SCROLL_MAX_DELTA) {
    max_delta = ZR_SCROLL_MAX_DELTA;
  }

  for (uint32_t delta = 1u; delta <= max_delta; delta++) {
    zr_scroll_scan_delta_dir(prev, next, delta, true, &best);
    zr_scroll_scan_delta_dir(prev, next, delta, false, &best);
  }

  if (!best.active) {
    return best;
  }

  /* Require a valid region: (bottom-top+1) > delta. */
  if (best.bottom <= best.top) {
    memset(&best, 0, sizeof(best));
    return best;
  }
  if ((best.bottom - best.top + 1u) <= best.lines) {
    memset(&best, 0, sizeof(best));
    return best;
  }
  if (best.lines == 0u) {
    memset(&best, 0, sizeof(best));
    return best;
  }

  return best;
}

static bool zr_emit_decstbm(zr_sb_t* sb, zr_term_state_t* ts, uint32_t top, uint32_t bottom) {
  if (!sb || !ts) {
    return false;
  }
  if (!zr_sb_write_u8(sb, 0x1Bu) || !zr_sb_write_u8(sb, (uint8_t)'[')) {
    return false;
  }
  if (!zr_sb_write_u32_dec(sb, top + 1u) || !zr_sb_write_u8(sb, (uint8_t)';') ||
      !zr_sb_write_u32_dec(sb, bottom + 1u) || !zr_sb_write_u8(sb, (uint8_t)'r')) {
    return false;
  }
  /* xterm/VT behavior: setting scroll margins homes the cursor. */
  ts->cursor_x = 0u;
  ts->cursor_y = 0u;
  return true;
}

static bool zr_emit_scroll_op(zr_sb_t* sb, zr_term_state_t* ts, bool up, uint32_t lines) {
  if (!sb || !ts) {
    return false;
  }
  if (lines == 0u) {
    return true;
  }
  if (!zr_sb_write_u8(sb, 0x1Bu) || !zr_sb_write_u8(sb, (uint8_t)'[')) {
    return false;
  }
  if (!zr_sb_write_u32_dec(sb, lines) || !zr_sb_write_u8(sb, up ? (uint8_t)'S' : (uint8_t)'T')) {
    return false;
  }
  return true;
}

static bool zr_emit_decstbm_reset(zr_sb_t* sb, zr_term_state_t* ts) {
  if (!sb || !ts) {
    return false;
  }
  if (!zr_sb_write_u8(sb, 0x1Bu) || !zr_sb_write_u8(sb, (uint8_t)'[') || !zr_sb_write_u8(sb, (uint8_t)'r')) {
    return false;
  }
  ts->cursor_x = 0u;
  ts->cursor_y = 0u;
  return true;
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
    const zr_cell_t* c = zr_fb_cell_const(ctx->next, xx, y);
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

static zr_result_t zr_diff_render_full_line(zr_diff_ctx_t* ctx, uint32_t y) {
  if (!ctx || !ctx->next) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (ctx->next->cols == 0u) {
    return ZR_OK;
  }
  return zr_diff_render_span(ctx, y, 0u, ctx->next->cols - 1u);
}

static void zr_diff_expand_span_for_wide(const zr_fb_t* next, uint32_t y, uint32_t* inout_start, uint32_t* inout_end) {
  if (!next || !inout_start || !inout_end) {
    return;
  }
  if (next->cols == 0u || y >= next->rows) {
    return;
  }

  uint32_t start = *inout_start;
  uint32_t end = *inout_end;
  if (start >= next->cols || end >= next->cols) {
    return;
  }

  if (start > 0u) {
    const zr_cell_t* c = zr_fb_cell_const(next, start, y);
    if (zr_cell_is_continuation(c)) {
      start--;
    }
  }

  if (end + 1u < next->cols) {
    const uint8_t w = zr_cell_width_in_next(next, end, y);
    if (w == 2u) {
      end++;
    }
  }

  *inout_start = start;
  *inout_end = end;
}

static uint32_t zr_u32_mul_clamp(uint32_t a, uint32_t b) {
  size_t prod = 0u;
  if (!zr_checked_mul_size((size_t)a, (size_t)b, &prod)) {
    return 0xFFFFFFFFu;
  }
  return (prod > (size_t)0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)prod;
}

static zr_result_t zr_diff_build_damage(zr_diff_ctx_t* ctx, const zr_limits_t* lim, zr_damage_rect_t* scratch,
                                        uint32_t scratch_cap) {
  if (!ctx || !ctx->prev || !ctx->next || !lim || !scratch) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (scratch_cap < lim->diff_max_damage_rects) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_damage_begin_frame(&ctx->damage, scratch, lim->diff_max_damage_rects, ctx->next->cols, ctx->next->rows);

  for (uint32_t y = 0u; y < ctx->next->rows; y++) {
    bool line_dirty = false;

    uint32_t x = 0u;
    while (x < ctx->next->cols) {
      if (!zr_line_dirty_at(ctx->prev, ctx->next, x, y)) {
        x++;
        continue;
      }

      uint32_t start = x;
      while (x < ctx->next->cols && zr_line_dirty_at(ctx->prev, ctx->next, x, y)) {
        x++;
      }
      uint32_t end = (x == 0u) ? 0u : (x - 1u);

      zr_diff_expand_span_for_wide(ctx->next, y, &start, &end);
      zr_damage_add_span(&ctx->damage, y, start, end);

      line_dirty = true;
      ctx->stats.dirty_cells += (end - start + 1u);

      if (ctx->damage.full_frame != 0u) {
        break;
      }
    }

    if (line_dirty) {
      ctx->stats.dirty_lines++;
    }
    if (ctx->damage.full_frame != 0u) {
      break;
    }
  }

  ctx->stats.damage_rects = ctx->damage.rect_count;
  ctx->stats.damage_cells = zr_damage_cells(&ctx->damage);
  ctx->stats.damage_full_frame = ctx->damage.full_frame;
  ctx->stats._pad0[0] = 0u;
  ctx->stats._pad0[1] = 0u;
  ctx->stats._pad0[2] = 0u;

  return ZR_OK;
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

/*
 * Try to apply a scroll-region optimization and report a row range to skip.
 *
 * Why: After emitting a terminal scroll for the moved block and redrawing the
 * newly exposed lines, the scrolled region is already synchronized with `next`,
 * so the normal per-row diff can skip it entirely.
 */
static zr_result_t zr_diff_try_scroll_opt(zr_diff_ctx_t* ctx, bool* out_skip, uint32_t* out_skip_top,
                                          uint32_t* out_skip_bottom) {
  if (!ctx || !out_skip || !out_skip_top || !out_skip_bottom) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_skip = false;
  *out_skip_top = 0u;
  *out_skip_bottom = 0u;

  const zr_scroll_plan_t plan = zr_diff_detect_scroll_fullwidth(ctx->prev, ctx->next);
  if (!plan.active) {
    return ZR_OK;
  }

  if (!zr_emit_decstbm(&ctx->sb, &ctx->ts, plan.top, plan.bottom)) {
    return ZR_ERR_LIMIT;
  }
  if (!zr_emit_scroll_op(&ctx->sb, &ctx->ts, plan.up, plan.lines)) {
    return ZR_ERR_LIMIT;
  }
  if (!zr_emit_decstbm_reset(&ctx->sb, &ctx->ts)) {
    return ZR_ERR_LIMIT;
  }

  /*
    After the terminal scroll, only the newly exposed lines need redraw.
    Redraw the full width to avoid relying on terminal-inserted blank style.
  */
  if (plan.up) {
    const uint32_t first_new = plan.bottom - plan.lines + 1u;
    for (uint32_t y = first_new; y <= plan.bottom; y++) {
      const zr_result_t rc = zr_diff_render_full_line(ctx, y);
      if (rc != ZR_OK) {
        return rc;
      }
      ctx->stats.dirty_lines++;
      ctx->stats.dirty_cells += ctx->next->cols;
    }
  } else {
    const uint32_t last_new = plan.top + plan.lines - 1u;
    for (uint32_t y = plan.top; y <= last_new; y++) {
      const zr_result_t rc = zr_diff_render_full_line(ctx, y);
      if (rc != ZR_OK) {
        return rc;
      }
      ctx->stats.dirty_lines++;
      ctx->stats.dirty_cells += ctx->next->cols;
    }
  }

  *out_skip = true;
  *out_skip_top = plan.top;
  *out_skip_bottom = plan.bottom;
  return zr_sb_truncated(&ctx->sb) ? ZR_ERR_LIMIT : ZR_OK;
}

zr_result_t zr_diff_render(const zr_fb_t* prev, const zr_fb_t* next, const plat_caps_t* caps,
                           const zr_term_state_t* initial_term_state, const zr_cursor_state_t* desired_cursor_state,
                           const zr_limits_t* lim, zr_damage_rect_t* scratch_damage_rects,
                           uint32_t scratch_damage_rect_cap, uint8_t enable_scroll_optimizations, uint8_t* out_buf,
                           size_t out_cap, size_t* out_len, zr_term_state_t* out_final_term_state,
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

  const zr_result_t arg_rc = zr_diff_validate_args(
      prev, next, caps, initial_term_state, desired_cursor_state, lim, scratch_damage_rects, scratch_damage_rect_cap,
      enable_scroll_optimizations, out_buf, out_len, out_final_term_state, out_stats);
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

  bool skip = false;
  uint32_t skip_top = 0u;
  uint32_t skip_bottom = 0u;
  if (enable_scroll_optimizations != 0u && caps->supports_scroll_region != 0u) {
    const zr_result_t rc = zr_diff_try_scroll_opt(&ctx, &skip, &skip_top, &skip_bottom);
    if (rc != ZR_OK) {
      zr_diff_zero_outputs(out_len, out_final_term_state, out_stats);
      return rc;
    }
  }

  if (skip) {
    /* Conservative: treat scroll-move frames as full-frame damage for metrics. */
    ctx.stats.damage_full_frame = 1u;
    ctx.stats.damage_rects = 1u;
    ctx.stats.damage_cells = zr_u32_mul_clamp(next->cols, next->rows);
    ctx.stats._pad0[0] = 0u;
    ctx.stats._pad0[1] = 0u;
    ctx.stats._pad0[2] = 0u;

    for (uint32_t y = 0u; y < next->rows; y++) {
      if (y >= skip_top && y <= skip_bottom) {
        continue;
      }
      const zr_result_t rc = zr_diff_render_line(&ctx, y);
      if (rc != ZR_OK) {
        zr_diff_zero_outputs(out_len, out_final_term_state, out_stats);
        return rc;
      }
    }
  } else {
    zr_result_t rc = zr_diff_build_damage(&ctx, lim, scratch_damage_rects, scratch_damage_rect_cap);
    if (rc != ZR_OK) {
      zr_diff_zero_outputs(out_len, out_final_term_state, out_stats);
      return rc;
    }

    if (ctx.damage.full_frame != 0u) {
      ctx.stats.dirty_lines = 0u;
      ctx.stats.dirty_cells = 0u;

      for (uint32_t y = 0u; y < next->rows; y++) {
        rc = zr_diff_render_line(&ctx, y);
        if (rc != ZR_OK) {
          zr_diff_zero_outputs(out_len, out_final_term_state, out_stats);
          return rc;
        }
      }
    } else {
      for (uint32_t i = 0u; i < ctx.damage.rect_count; i++) {
        const zr_damage_rect_t* r = &ctx.damage.rects[i];
        for (uint32_t y = r->y0; y <= r->y1; y++) {
          rc = zr_diff_render_span(&ctx, y, r->x0, r->x1);
          if (rc != ZR_OK) {
            zr_diff_zero_outputs(out_len, out_final_term_state, out_stats);
            return rc;
          }
        }
      }
    }
  }

  if (!zr_emit_cursor_desired(&ctx.sb, &ctx.ts, desired_cursor_state, next, caps)) {
    zr_diff_zero_outputs(out_len, out_final_term_state, out_stats);
    return ZR_ERR_LIMIT;
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
