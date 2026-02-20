/*
  src/core/zr_blit_quadrant.c — Quadrant blitter (2x2 sub-cells).

  Why: Uses deterministic two-color partitioning to map four sampled pixels
  into one Unicode quadrant glyph with foreground/background colors.
*/

#include "core/zr_blit.h"

#include <limits.h>

const zr_blit_glyph_t zr_blit_quadrant_glyphs[ZR_BLIT_QUADRANT_GLYPHS] = {
    {{0x20u, 0x00u, 0x00u, 0x00u}, 1u, {0u, 0u, 0u}}, /* 0x0 -> U+0020 */
    {{0xE2u, 0x96u, 0x98u, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0x1 -> U+2598 */
    {{0xE2u, 0x96u, 0x9Du, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0x2 -> U+259D */
    {{0xE2u, 0x96u, 0x80u, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0x3 -> U+2580 */
    {{0xE2u, 0x96u, 0x96u, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0x4 -> U+2596 */
    {{0xE2u, 0x96u, 0x8Cu, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0x5 -> U+258C */
    {{0xE2u, 0x96u, 0x9Eu, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0x6 -> U+259E */
    {{0xE2u, 0x96u, 0x9Bu, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0x7 -> U+259B */
    {{0xE2u, 0x96u, 0x97u, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0x8 -> U+2597 */
    {{0xE2u, 0x96u, 0x9Au, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0x9 -> U+259A */
    {{0xE2u, 0x96u, 0x90u, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0xA -> U+2590 */
    {{0xE2u, 0x96u, 0x9Cu, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0xB -> U+259C */
    {{0xE2u, 0x96u, 0x84u, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0xC -> U+2584 */
    {{0xE2u, 0x96u, 0x99u, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0xD -> U+2599 */
    {{0xE2u, 0x96u, 0x9Fu, 0x00u}, 3u, {0u, 0u, 0u}}, /* 0xE -> U+259F */
    {{0xE2u, 0x96u, 0x88u, 0x00u}, 3u, {0u, 0u, 0u}}  /* 0xF -> U+2588 */
};

static uint32_t zr_blit_quadrant_cell_bg(const zr_fb_painter_t* painter, int32_t x, int32_t y) {
  const zr_cell_t* c = zr_fb_cell_const(painter->fb, (uint32_t)x, (uint32_t)y);
  if (!c) {
    return 0u;
  }
  return c->style.bg_rgb;
}

static uint32_t zr_blit_quadrant_mean(const uint32_t colors[4], uint8_t mask, uint8_t want_set, uint8_t* out_count) {
  uint32_t r = 0u;
  uint32_t g = 0u;
  uint32_t b = 0u;
  uint8_t count = 0u;

  for (uint8_t i = 0u; i < ZR_BLIT_QUADRANT_SUBPIXELS; i++) {
    const uint8_t bit = (uint8_t)((mask >> i) & 1u);
    if (bit == want_set) {
      const uint32_t rgb = colors[i];
      r += (rgb >> 16u) & 0xFFu;
      g += (rgb >> 8u) & 0xFFu;
      b += rgb & 0xFFu;
      count++;
    }
  }

  *out_count = count;
  if (count == 0u) {
    return 0u;
  }
  return zr_blit_pack_rgb((uint8_t)(r / count), (uint8_t)(g / count), (uint8_t)(b / count));
}

static uint64_t zr_blit_quadrant_error(const uint32_t colors[4], uint8_t mask, uint32_t fg, uint32_t bg) {
  uint64_t err = 0u;
  for (uint8_t i = 0u; i < ZR_BLIT_QUADRANT_SUBPIXELS; i++) {
    const uint8_t bit = (uint8_t)((mask >> i) & 1u);
    err += zr_blit_rgb_distance_sq(colors[i], bit != 0u ? fg : bg);
  }
  return err;
}

/* Search all 16 patterns and pick the minimum-error 2-color partition. */
static void zr_blit_quadrant_partition(const uint32_t colors[4], uint8_t* out_mask, uint32_t* out_fg,
                                       uint32_t* out_bg) {
  uint64_t best_err = UINT64_MAX;
  uint8_t best_mask = 0u;
  uint32_t best_fg = 0u;
  uint32_t best_bg = 0u;

  for (uint8_t mask = 0u; mask < 16u; mask++) {
    uint8_t fg_count = 0u;
    uint8_t bg_count = 0u;
    uint32_t fg = zr_blit_quadrant_mean(colors, mask, 1u, &fg_count);
    uint32_t bg = zr_blit_quadrant_mean(colors, mask, 0u, &bg_count);
    const uint64_t err = zr_blit_quadrant_error(colors, mask, fg, bg);

    if (fg_count == 0u) {
      fg = bg;
    }
    if (bg_count == 0u) {
      bg = fg;
    }

    if (err < best_err || (err == best_err && mask < best_mask)) {
      best_err = err;
      best_mask = mask;
      best_fg = fg;
      best_bg = bg;
    }
  }

  *out_mask = best_mask;
  *out_fg = best_fg;
  *out_bg = best_bg;
}

static void zr_blit_quadrant_style(zr_style_t* out_style, uint32_t fg, uint32_t bg) {
  out_style->fg_rgb = fg;
  out_style->bg_rgb = bg;
  out_style->attrs = 0u;
  out_style->reserved = 0u;
  out_style->underline_rgb = 0u;
  out_style->link_ref = 0u;
}

zr_result_t zr_blit_quadrant(zr_fb_painter_t* painter, zr_rect_t dst_rect, const zr_blit_input_t* input) {
  for (int32_t y = 0; y < dst_rect.h; y++) {
    for (int32_t x = 0; x < dst_rect.w; x++) {
      uint32_t colors[4];
      uint8_t opaque_count = 0u;
      const int32_t dst_x = dst_rect.x + x;
      const int32_t dst_y = dst_rect.y + y;
      const uint32_t under_bg = zr_blit_quadrant_cell_bg(painter, dst_x, dst_y);

      for (uint8_t i = 0u; i < 4u; i++) {
        uint8_t rgba[4];
        const uint32_t sx = (uint32_t)x * 2u + ((i == 1u || i == 3u) ? 1u : 0u);
        const uint32_t sy = (uint32_t)y * 2u + ((i >= 2u) ? 1u : 0u);
        const zr_result_t rc =
            zr_blit_sample_subpixel(input, sx, sy, (uint32_t)dst_rect.w, (uint32_t)dst_rect.h, 2u, 2u, rgba);
        if (rc != ZR_OK) {
          return rc;
        }
        if (zr_blit_alpha_is_opaque(rgba[3]) != 0u) {
          colors[i] = zr_blit_pack_rgb(rgba[0], rgba[1], rgba[2]);
          opaque_count++;
        } else {
          colors[i] = under_bg;
        }
      }

      if (opaque_count == 0u) {
        continue;
      }

      uint8_t mask = 0u;
      uint32_t fg = 0u;
      uint32_t bg = 0u;
      zr_style_t style;
      zr_blit_quadrant_partition(colors, &mask, &fg, &bg);
      zr_blit_quadrant_style(&style, fg, bg);
      (void)zr_blit_put_glyph(painter, dst_x, dst_y, &zr_blit_quadrant_glyphs[mask], &style);
    }
  }

  return ZR_OK;
}
