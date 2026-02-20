/*
  tests/unit/test_blit_quadrant.c — Unit tests for quadrant (2x2) blitter.

  Why: Pins deterministic partition outcomes for common two-color patterns.
*/

#include "zr_test.h"

#include "core/zr_blit.h"
#include "core/zr_framebuffer.h"

ZR_TEST_UNIT(blit_quadrant_vertical_split_maps_to_left_half_block) {
  uint8_t pixels[16] = {
      255u, 0u, 0u, 255u, 0u, 0u, 255u, 255u, 255u, 0u, 0u, 255u, 0u, 0u, 255u, 255u,
  };
  zr_blit_input_t in = {pixels, 2u, 2u, 8u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_blit_quadrant(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xE2u);
  ZR_ASSERT_EQ_U32(c->glyph[1], 0x96u);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0x8Cu); /* U+258C */
  ZR_ASSERT_EQ_U32(c->style.fg_rgb, 0x00FF0000u);
  ZR_ASSERT_EQ_U32(c->style.bg_rgb, 0x000000FFu);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(blit_quadrant_checkerboard_tie_break_is_deterministic) {
  uint8_t pixels[16] = {
      255u, 255u, 255u, 255u, 0u, 0u, 0u, 255u, 0u, 0u, 0u, 255u, 255u, 255u, 255u, 255u,
  };
  zr_blit_input_t in = {pixels, 2u, 2u, 8u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_blit_quadrant(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);
  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0x9Eu); /* U+259E from mask 0x6 tie-break */

  /* Re-run and assert byte-identical determinism. */
  const zr_cell_t first = *c;
  ZR_ASSERT_EQ_U32(zr_blit_quadrant(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);
  c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_MEMEQ(&first, c, sizeof(first));
  zr_fb_release(&fb);
}
