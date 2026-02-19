/*
  src/core/zr_blit_ascii.c — ASCII fallback blitter (1x1, space+bg).

  Why: Provides the most compatible terminal fallback path when block glyph
  rendering is unavailable or explicitly disabled.
*/

#include "core/zr_blit.h"

/* Build style for ASCII fallback where only background color is visible. */
static void zr_blit_ascii_style(zr_style_t* out_style, uint32_t rgb) {
  out_style->fg_rgb = rgb;
  out_style->bg_rgb = rgb;
  out_style->attrs = 0u;
  out_style->reserved = 0u;
  out_style->underline_rgb = 0u;
  out_style->link_ref = 0u;
}

zr_result_t zr_blit_ascii(zr_fb_painter_t* painter, zr_rect_t dst_rect, const zr_blit_input_t* input) {
  const zr_blit_glyph_t* glyph = &zr_blit_halfblock_glyphs[ZR_BLIT_HALF_GLYPH_SPACE];

  for (int32_t y = 0; y < dst_rect.h; y++) {
    for (int32_t x = 0; x < dst_rect.w; x++) {
      uint8_t rgba[4];
      zr_style_t style;
      const uint32_t sub_x = (uint32_t)x;
      const uint32_t sub_y = (uint32_t)y;
      const zr_result_t rc =
          zr_blit_sample_subpixel(input, sub_x, sub_y, (uint32_t)dst_rect.w, (uint32_t)dst_rect.h, 1u, 1u, rgba);
      if (rc != ZR_OK) {
        return rc;
      }

      if (zr_blit_alpha_is_opaque(rgba[3]) == 0u) {
        continue;
      }

      zr_blit_ascii_style(&style, zr_blit_pack_rgb(rgba[0], rgba[1], rgba[2]));
      (void)zr_blit_put_glyph(painter, dst_rect.x + x, dst_rect.y + y, glyph, &style);
    }
  }

  return ZR_OK;
}
