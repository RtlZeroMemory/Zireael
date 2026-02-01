/*
  tests/unit/test_framebuffer_init_resize.c — Framebuffer init/release/resize contracts.

  Why: Validates basic lifecycle behavior and the "no partial effects" guarantee
  for zr_fb_resize failure paths.
*/

#include "zr_test.h"

#include "core/zr_framebuffer.h"

#include <string.h>

static zr_style_t zr_style0(void) {
  zr_style_t s;
  s.fg_rgb = 0u;
  s.bg_rgb = 0u;
  s.attrs = 0u;
  s.reserved = 0u;
  return s;
}

ZR_TEST_UNIT(framebuffer_init_release_basics) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 3u, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(fb.cols, 3u);
  ZR_ASSERT_EQ_U32(fb.rows, 2u);
  ZR_ASSERT_TRUE(fb.cells != NULL);

  zr_fb_release(&fb);
  ZR_ASSERT_EQ_U32(fb.cols, 0u);
  ZR_ASSERT_EQ_U32(fb.rows, 0u);
  ZR_ASSERT_TRUE(fb.cells == NULL);
}

ZR_TEST_UNIT(framebuffer_resize_failure_has_no_partial_effects) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 2u, 2u), ZR_OK);
  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, &s0), ZR_OK);

  zr_cell_t* c = zr_fb_cell(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = (uint8_t)'X';
  c->glyph_len = 1u;
  c->width = 1u;

  /* Deterministic failure path (limit): dims exceed the supported range. */
  const zr_result_t rc = zr_fb_resize(&fb, 0xFFFFFFFFu, 1u);
  ZR_ASSERT_TRUE(rc == ZR_ERR_LIMIT);

  ZR_ASSERT_EQ_U32(fb.cols, 2u);
  ZR_ASSERT_EQ_U32(fb.rows, 2u);

  const zr_cell_t* c2 = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c2 != NULL);
  ZR_ASSERT_EQ_U32(c2->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c2->glyph[0], (uint8_t)'X');

  zr_fb_release(&fb);
}
