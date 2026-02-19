/*
  tests/unit/test_blit_ascii.c — Unit tests for ASCII fallback blitter.

  Why: Ensures 1x1 fallback path writes deterministic space+background cells.
*/

#include "zr_test.h"

#include "core/zr_blit.h"
#include "core/zr_framebuffer.h"

#include <string.h>

static void zr_test_set_cell_bg(zr_fb_t* fb, uint32_t x, uint32_t y, uint32_t bg) {
  zr_cell_t* c = zr_fb_cell(fb, x, y);
  if (!c) {
    return;
  }
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = (uint8_t)' ';
  c->glyph_len = 1u;
  c->width = 1u;
  c->style.fg_rgb = 0u;
  c->style.bg_rgb = bg;
  c->style.attrs = 0u;
  c->style.reserved = 0u;
  c->style.underline_rgb = 0u;
  c->style.link_ref = 0u;
}

ZR_TEST_UNIT(blit_ascii_writes_space_with_pixel_background) {
  uint8_t pixels[4] = {10u, 20u, 30u, 255u};
  zr_blit_input_t in = {pixels, 1u, 1u, 4u};
  zr_fb_t fb;
  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, clip_stack, 2u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_blit_ascii(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], (uint8_t)' ');
  ZR_ASSERT_EQ_U32(c->style.bg_rgb, 0x000A141Eu);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(blit_ascii_transparent_pixel_preserves_existing_cell) {
  uint8_t pixels[4] = {1u, 2u, 3u, 0u};
  zr_blit_input_t in = {pixels, 1u, 1u, 4u};
  zr_fb_t fb;
  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  zr_test_set_cell_bg(&fb, 0u, 0u, 0x00112233u);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, clip_stack, 2u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_blit_ascii(&p, (zr_rect_t){0, 0, 1, 1}, &in), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->style.bg_rgb, 0x00112233u);
  zr_fb_release(&fb);
}
