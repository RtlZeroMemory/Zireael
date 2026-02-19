/*
  tests/unit/test_blit_sextant.c — Unit tests for sextant (2x3) blitter.

  Why: Verifies fallback masks and deterministic partition behavior.
*/

#include "zr_test.h"

#include "core/zr_blit.h"
#include "core/zr_framebuffer.h"

ZR_TEST_UNIT(blit_sextant_left_column_maps_to_left_half_fallback) {
  uint8_t pixels[24] = {
      255u, 255u, 255u, 255u, 0u,   0u,   0u,   255u,
      255u, 255u, 255u, 255u, 0u,   0u,   0u,   255u,
      255u, 255u, 255u, 255u, 0u,   0u,   0u,   255u,
  };
  zr_blit_input_t in = {pixels, 2u, 3u, 8u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_blit_sextant(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xE2u);
  ZR_ASSERT_EQ_U32(c->glyph[1], 0x96u);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0x8Cu); /* U+258C fallback for mask 21 */
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(blit_sextant_determinism_same_input_same_cell_output) {
  uint8_t pixels[24] = {
      255u, 0u,   0u,   255u, 0u,   0u,   255u, 255u,
      0u,   255u, 0u,   255u, 255u, 255u, 0u,   255u,
      0u,   255u, 255u, 255u, 255u, 0u,   255u, 255u,
  };
  zr_blit_input_t in = {pixels, 2u, 3u, 8u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_blit_sextant(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);
  const zr_cell_t first = *zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_EQ_U32(zr_blit_sextant(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);
  const zr_cell_t* second = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_MEMEQ(&first, second, sizeof(first));
  zr_fb_release(&fb);
}
