/*
  tests/unit/test_blit_braille.c — Unit tests for braille (2x4) blitter.

  Why: Pins braille bit layout and alpha-threshold behavior.
*/

#include "zr_test.h"

#include "core/zr_blit.h"
#include "core/zr_framebuffer.h"

#include <string.h>

static void zr_braille_seed_space_cell(zr_fb_t* fb) {
  zr_cell_t* c = zr_fb_cell(fb, 0u, 0u);
  if (!c) {
    return;
  }
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = (uint8_t)' ';
  c->glyph_len = 1u;
  c->width = 1u;
  c->style.fg_rgb = 0u;
  c->style.bg_rgb = 0x00112233u;
  c->style.attrs = 0u;
  c->style.reserved = 0u;
  c->style.underline_rgb = 0u;
  c->style.link_ref = 0u;
}

ZR_TEST_UNIT(blit_braille_single_white_pixel_sets_dot1) {
  uint8_t pixels[32] = {0u};
  for (int i = 0; i < 8; i++) {
    pixels[i * 4 + 3] = 255u;
  }
  pixels[0] = 255u;
  pixels[1] = 255u;
  pixels[2] = 255u;

  zr_blit_input_t in = {pixels, 2u, 4u, 8u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_blit_braille(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xE2u);
  ZR_ASSERT_EQ_U32(c->glyph[1], 0xA0u);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0x81u); /* U+2801 */
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(blit_braille_all_transparent_preserves_cell) {
  uint8_t pixels[32] = {0u};
  zr_blit_input_t in = {pixels, 2u, 4u, 8u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  zr_braille_seed_space_cell(&fb);
  const zr_cell_t before = *zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_blit_braille(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);

  const zr_cell_t* after = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_MEMEQ(&before, after, sizeof(before));
  zr_fb_release(&fb);
}
