/*
  src/core/zr_blit_halfblock.c — Halfblock blitter (1x2 sub-cells).

  Why: Delivers broad Unicode compatibility with a simple two-sample partition
  that maps vertical pixel pairs to block-element glyphs.
*/

#include "core/zr_blit.h"

#define ZR_BLIT_HALFBLOCK_EQUAL_TOL_SQ 256u

static uint32_t zr_blit_halfblock_cell_bg(const zr_fb_painter_t* painter, int32_t x, int32_t y) {
  const zr_cell_t* c = zr_fb_cell_const(painter->fb, (uint32_t)x, (uint32_t)y);
  if (!c) {
    return 0u;
  }
  return c->style.bg_rgb;
}

static void zr_blit_halfblock_style(zr_style_t* out_style, uint32_t fg, uint32_t bg) {
  out_style->fg_rgb = fg;
  out_style->bg_rgb = bg;
  out_style->attrs = 0u;
  out_style->reserved = 0u;
  out_style->underline_rgb = 0u;
  out_style->link_ref = 0u;
}

static const zr_blit_glyph_t* zr_blit_halfblock_pick_glyph(uint32_t top_rgb, uint32_t bot_rgb, uint8_t top_opaque,
                                                           uint8_t bot_opaque, zr_style_t* out_style) {
  const uint32_t dist = zr_blit_rgb_distance_sq(top_rgb, bot_rgb);
  if (dist <= ZR_BLIT_HALFBLOCK_EQUAL_TOL_SQ) {
    zr_blit_halfblock_style(out_style, top_rgb, top_rgb);
    return &zr_blit_halfblock_glyphs[ZR_BLIT_HALF_GLYPH_SPACE];
  }

  if (top_opaque == 0u && bot_opaque != 0u) {
    zr_blit_halfblock_style(out_style, bot_rgb, top_rgb);
    return &zr_blit_halfblock_glyphs[ZR_BLIT_HALF_GLYPH_LOWER];
  }
  if (bot_opaque == 0u && top_opaque != 0u) {
    zr_blit_halfblock_style(out_style, top_rgb, bot_rgb);
    return &zr_blit_halfblock_glyphs[ZR_BLIT_HALF_GLYPH_UPPER];
  }

  if (zr_blit_luma_bt709(top_rgb) >= zr_blit_luma_bt709(bot_rgb)) {
    zr_blit_halfblock_style(out_style, top_rgb, bot_rgb);
    return &zr_blit_halfblock_glyphs[ZR_BLIT_HALF_GLYPH_UPPER];
  }

  zr_blit_halfblock_style(out_style, bot_rgb, top_rgb);
  return &zr_blit_halfblock_glyphs[ZR_BLIT_HALF_GLYPH_LOWER];
}

zr_result_t zr_blit_halfblock(zr_fb_painter_t* painter, zr_rect_t dst_rect, const zr_blit_input_t* input) {
  for (int32_t y = 0; y < dst_rect.h; y++) {
    for (int32_t x = 0; x < dst_rect.w; x++) {
      uint8_t top_rgba[4];
      uint8_t bot_rgba[4];
      const int32_t dst_x = dst_rect.x + x;
      const int32_t dst_y = dst_rect.y + y;
      const uint32_t bg_under = zr_blit_halfblock_cell_bg(painter, dst_x, dst_y);
      const zr_result_t rc_top = zr_blit_sample_subpixel(input, (uint32_t)x, (uint32_t)(y * 2), (uint32_t)dst_rect.w,
                                                         (uint32_t)dst_rect.h, 1u, 2u, top_rgba);
      const zr_result_t rc_bot = zr_blit_sample_subpixel(input, (uint32_t)x, (uint32_t)(y * 2 + 1),
                                                         (uint32_t)dst_rect.w, (uint32_t)dst_rect.h, 1u, 2u, bot_rgba);
      if (rc_top != ZR_OK || rc_bot != ZR_OK) {
        return (rc_top != ZR_OK) ? rc_top : rc_bot;
      }

      const uint8_t top_opaque = zr_blit_alpha_is_opaque(top_rgba[3]);
      const uint8_t bot_opaque = zr_blit_alpha_is_opaque(bot_rgba[3]);
      if (top_opaque == 0u && bot_opaque == 0u) {
        continue;
      }

      const uint32_t top_rgb = top_opaque != 0u ? zr_blit_pack_rgb(top_rgba[0], top_rgba[1], top_rgba[2]) : bg_under;
      const uint32_t bot_rgb = bot_opaque != 0u ? zr_blit_pack_rgb(bot_rgba[0], bot_rgba[1], bot_rgba[2]) : bg_under;
      zr_style_t style;
      const zr_blit_glyph_t* glyph = zr_blit_halfblock_pick_glyph(top_rgb, bot_rgb, top_opaque, bot_opaque, &style);
      (void)zr_blit_put_glyph(painter, dst_x, dst_y, glyph, &style);
    }
  }

  return ZR_OK;
}
