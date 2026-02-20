/*
  src/core/zr_blit_sextant.c — Sextant blitter (2x3 sub-cells).

  Why: Uses deterministic two-color partitioning across six sampled sub-pixels
  for higher-resolution output on terminals with Unicode sextant support.
*/

#include "core/zr_blit.h"

#include <limits.h>

/*
  Sextant mask index -> UTF-8 glyph.
  Missing Unicode sextants use deterministic fallbacks:
    - 0x00 -> U+0020
    - 0x15 -> U+258C (left half)
    - 0x2A -> U+2590 (right half)
    - 0x3F -> U+2588 (full block)
*/
const zr_blit_glyph_t zr_blit_sextant_glyphs[ZR_BLIT_SEXTANT_GLYPHS] = {
    {{0x20u, 0x00u, 0x00u, 0x00u}, 1u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x80u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x81u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x82u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x83u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x84u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x85u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x86u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x87u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x88u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x89u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x8Au}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x8Bu}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x8Cu}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x8Du}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x8Eu}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x8Fu}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x90u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x91u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x92u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x93u}, 4u, {0u, 0u, 0u}}, {{0xE2u, 0x96u, 0x8Cu, 0x00u}, 3u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x94u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x95u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x96u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x97u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x98u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x99u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x9Au}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x9Bu}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x9Cu}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x9Du}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0x9Eu}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0x9Fu}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xA0u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xA1u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xA2u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xA3u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xA4u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xA5u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xA6u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xA7u}, 4u, {0u, 0u, 0u}},
    {{0xE2u, 0x96u, 0x90u, 0x00u}, 3u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xA8u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xA9u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xAAu}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xABu}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xACu}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xADu}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xAEu}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xAFu}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xB0u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xB1u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xB2u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xB3u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xB4u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xB5u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xB6u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xB7u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xB8u}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xB9u}, 4u, {0u, 0u, 0u}}, {{0xF0u, 0x9Fu, 0xACu, 0xBAu}, 4u, {0u, 0u, 0u}},
    {{0xF0u, 0x9Fu, 0xACu, 0xBBu}, 4u, {0u, 0u, 0u}}, {{0xE2u, 0x96u, 0x88u, 0x00u}, 3u, {0u, 0u, 0u}}};

static uint32_t zr_blit_sextant_cell_bg(const zr_fb_painter_t* painter, int32_t x, int32_t y) {
  const zr_cell_t* c = zr_fb_cell_const(painter->fb, (uint32_t)x, (uint32_t)y);
  if (!c) {
    return 0u;
  }
  return c->style.bg_rgb;
}

static uint32_t zr_blit_sextant_mean(const uint32_t colors[6], uint8_t mask, uint8_t want_set, uint8_t* out_count) {
  uint32_t r = 0u;
  uint32_t g = 0u;
  uint32_t b = 0u;
  uint8_t count = 0u;

  for (uint8_t i = 0u; i < ZR_BLIT_SEXTANT_SUBPIXELS; i++) {
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

static uint64_t zr_blit_sextant_error(const uint32_t colors[6], uint8_t mask, uint32_t fg, uint32_t bg) {
  uint64_t err = 0u;
  for (uint8_t i = 0u; i < ZR_BLIT_SEXTANT_SUBPIXELS; i++) {
    const uint8_t bit = (uint8_t)((mask >> i) & 1u);
    err += zr_blit_rgb_distance_sq(colors[i], bit != 0u ? fg : bg);
  }
  return err;
}

/* Search all 64 sextant masks and pick the minimum-error partition. */
static void zr_blit_sextant_partition(const uint32_t colors[6], uint8_t* out_mask, uint32_t* out_fg, uint32_t* out_bg) {
  uint64_t best_err = UINT64_MAX;
  uint8_t best_mask = 0u;
  uint32_t best_fg = 0u;
  uint32_t best_bg = 0u;

  for (uint8_t mask = 0u; mask < 64u; mask++) {
    uint8_t fg_count = 0u;
    uint8_t bg_count = 0u;
    uint32_t fg = zr_blit_sextant_mean(colors, mask, 1u, &fg_count);
    uint32_t bg = zr_blit_sextant_mean(colors, mask, 0u, &bg_count);
    const uint64_t err = zr_blit_sextant_error(colors, mask, fg, bg);

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

static void zr_blit_sextant_style(zr_style_t* out_style, uint32_t fg, uint32_t bg) {
  out_style->fg_rgb = fg;
  out_style->bg_rgb = bg;
  out_style->attrs = 0u;
  out_style->reserved = 0u;
  out_style->underline_rgb = 0u;
  out_style->link_ref = 0u;
}

zr_result_t zr_blit_sextant(zr_fb_painter_t* painter, zr_rect_t dst_rect, const zr_blit_input_t* input) {
  for (int32_t y = 0; y < dst_rect.h; y++) {
    for (int32_t x = 0; x < dst_rect.w; x++) {
      uint32_t colors[6];
      uint8_t opaque_count = 0u;
      const int32_t dst_x = dst_rect.x + x;
      const int32_t dst_y = dst_rect.y + y;
      const uint32_t under_bg = zr_blit_sextant_cell_bg(painter, dst_x, dst_y);

      for (uint8_t i = 0u; i < 6u; i++) {
        uint8_t rgba[4];
        const uint32_t sx = (uint32_t)x * 2u + (uint32_t)(i & 1u);
        const uint32_t sy = (uint32_t)y * 3u + (uint32_t)(i / 2u);
        const zr_result_t rc =
            zr_blit_sample_subpixel(input, sx, sy, (uint32_t)dst_rect.w, (uint32_t)dst_rect.h, 2u, 3u, rgba);
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
      zr_blit_sextant_partition(colors, &mask, &fg, &bg);
      zr_blit_sextant_style(&style, fg, bg);
      (void)zr_blit_put_glyph(painter, dst_x, dst_y, &zr_blit_sextant_glyphs[mask], &style);
    }
  }

  return ZR_OK;
}
