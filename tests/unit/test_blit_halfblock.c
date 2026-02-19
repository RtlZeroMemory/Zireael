/*
  tests/unit/test_blit_halfblock.c — Unit tests for halfblock (1x2) blitter.

  Why: Locks basic glyph and color decisions for the compatibility blitter.
*/

#include "zr_test.h"

#include "core/zr_blit.h"
#include "core/zr_framebuffer.h"

#include <string.h>

ZR_TEST_UNIT(blit_halfblock_top_bottom_split_prefers_upper_for_brighter_top) {
  uint8_t pixels[8] = {
      255u, 0u,   0u,   255u, /* top red */
      0u,   0u,   255u, 255u  /* bottom blue */
  };
  zr_blit_input_t in = {pixels, 1u, 2u, 4u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_blit_halfblock(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xE2u);
  ZR_ASSERT_EQ_U32(c->glyph[1], 0x96u);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0x80u);
  ZR_ASSERT_EQ_U32(c->style.fg_rgb, 0x00FF0000u);
  ZR_ASSERT_EQ_U32(c->style.bg_rgb, 0x000000FFu);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(blit_halfblock_solid_color_collapses_to_space) {
  uint8_t pixels[8] = {
      10u, 20u, 30u, 255u,
      10u, 20u, 30u, 255u,
  };
  zr_blit_input_t in = {pixels, 1u, 2u, 4u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_blit_halfblock(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], (uint8_t)' ');
  ZR_ASSERT_EQ_U32(c->style.bg_rgb, 0x000A141Eu);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(blit_halfblock_alpha_threshold_127_transparent_128_opaque) {
  uint8_t pixels[8] = {
      255u, 255u, 255u, 127u, /* transparent top */
      0u,   255u, 0u,   128u, /* opaque bottom */
  };
  zr_blit_input_t in = {pixels, 1u, 2u, 4u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_blit_halfblock(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0x84u); /* U+2584 lower half */
  zr_fb_release(&fb);
}
