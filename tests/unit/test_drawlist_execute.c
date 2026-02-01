/*
  tests/unit/test_drawlist_execute.c — Unit tests for drawlist v1 execution into zr_fb_t.
*/

#include "zr_test.h"

#include "core/zr_drawlist.h"
#include "core/zr_framebuffer.h"

extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;
extern const uint8_t zr_test_dl_fixture2[];
extern const size_t zr_test_dl_fixture2_len;
extern const uint8_t zr_test_dl_fixture3[];
extern const size_t zr_test_dl_fixture3_len;
extern const uint8_t zr_test_dl_fixture4[];
extern const size_t zr_test_dl_fixture4_len;

static void zr_assert_cell_glyph(zr_test_ctx_t* ctx, const zr_cell_t* c, uint8_t byte) {
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], byte);
  ZR_ASSERT_EQ_U32(c->width, 1u);
}

ZR_TEST_UNIT(drawlist_execute_fixture1_text_written) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &lim, &v), ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim), ZR_OK);

  const zr_cell_t* c1 = zr_fb_cell_const(&fb, 1u, 0u);
  const zr_cell_t* c2 = zr_fb_cell_const(&fb, 2u, 0u);
  zr_assert_cell_glyph(ctx, c1, (uint8_t)'H');
  zr_assert_cell_glyph(ctx, c2, (uint8_t)'i');
  ZR_ASSERT_EQ_U32(c1->style.fg_rgb, 0x01020304u);
  ZR_ASSERT_EQ_U32(c1->style.bg_rgb, 0x0A0B0C0Du);
  ZR_ASSERT_EQ_U32(c1->style.attrs, 0x00000011u);
  ZR_ASSERT_EQ_U32(c1->style.reserved, 0u);

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_execute_fixture2_clip_applies) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture2, zr_test_dl_fixture2_len, &lim, &v), ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 3u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim), ZR_OK);

  const zr_cell_t* in0 = zr_fb_cell_const(&fb, 1u, 1u);
  const zr_cell_t* in1 = zr_fb_cell_const(&fb, 2u, 1u);
  const zr_cell_t* out0 = zr_fb_cell_const(&fb, 0u, 0u);

  ZR_ASSERT_EQ_U32(in0->style.fg_rgb, 0x11111111u);
  ZR_ASSERT_EQ_U32(in0->style.bg_rgb, 0x22222222u);
  ZR_ASSERT_EQ_U32(in1->style.fg_rgb, 0x11111111u);
  ZR_ASSERT_EQ_U32(out0->style.fg_rgb, 0u);

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_execute_fixture3_text_run_segments) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture3, zr_test_dl_fixture3_len, &lim, &v), ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 8u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim), ZR_OK);

  const zr_cell_t* a = zr_fb_cell_const(&fb, 0u, 0u);
  const zr_cell_t* d = zr_fb_cell_const(&fb, 3u, 0u);
  zr_assert_cell_glyph(ctx, a, (uint8_t)'A');
  zr_assert_cell_glyph(ctx, d, (uint8_t)'D');
  ZR_ASSERT_EQ_U32(a->style.fg_rgb, 1u);
  ZR_ASSERT_EQ_U32(d->style.fg_rgb, 3u);

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_execute_clip_does_not_change_wide_cursor_advance) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture4, zr_test_dl_fixture4_len, &lim, &v), ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim), ZR_OK);

  /*
    The clip only includes x==1. The drawlist places a wide glyph starting at x==0
    followed by 'A'. If wide-glyph advance were reduced to 1 due to clipping,
    'A' would be drawn into the visible cell x==1.
  */
  const zr_cell_t* c = zr_fb_cell_const(&fb, 1u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], (uint8_t)' ');

  zr_fb_release(&fb);
}
