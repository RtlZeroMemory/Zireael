/*
  tests/unit/test_drawlist_execute.c — Unit tests for drawlist v1 execution into zr_fb_t.
*/

#include "zr_test.h"

#include "core/zr_drawlist.h"
#include "core/zr_fb.h"

extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;
extern const uint8_t zr_test_dl_fixture2[];
extern const size_t zr_test_dl_fixture2_len;
extern const uint8_t zr_test_dl_fixture3[];
extern const size_t zr_test_dl_fixture3_len;

static void zr_assert_cell_glyph(zr_test_ctx_t* ctx, const zr_fb_cell_t* c, uint8_t byte) {
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], byte);
}

ZR_TEST_UNIT(drawlist_execute_fixture1_text_written) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &lim, &v), ZR_OK);

  zr_fb_cell_t cells[4 * 2];
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, cells, 4u, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim), ZR_OK);

  const zr_fb_cell_t* c1 = zr_fb_cell_at_const(&fb, 1u, 0u);
  const zr_fb_cell_t* c2 = zr_fb_cell_at_const(&fb, 2u, 0u);
  zr_assert_cell_glyph(ctx, c1, (uint8_t)'H');
  zr_assert_cell_glyph(ctx, c2, (uint8_t)'i');
  ZR_ASSERT_EQ_U32(c1->style.fg, 0x01020304u);
  ZR_ASSERT_EQ_U32(c1->style.bg, 0x0A0B0C0Du);
  ZR_ASSERT_EQ_U32(c1->style.attrs, 0x00000011u);
}

ZR_TEST_UNIT(drawlist_execute_fixture2_clip_applies) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture2, zr_test_dl_fixture2_len, &lim, &v), ZR_OK);

  zr_fb_cell_t cells[4 * 3];
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, cells, 4u, 3u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim), ZR_OK);

  const zr_fb_cell_t* in0 = zr_fb_cell_at_const(&fb, 1u, 1u);
  const zr_fb_cell_t* in1 = zr_fb_cell_at_const(&fb, 2u, 1u);
  const zr_fb_cell_t* out0 = zr_fb_cell_at_const(&fb, 0u, 0u);

  ZR_ASSERT_EQ_U32(in0->style.fg, 0x11111111u);
  ZR_ASSERT_EQ_U32(in0->style.bg, 0x22222222u);
  ZR_ASSERT_EQ_U32(in1->style.fg, 0x11111111u);
  ZR_ASSERT_EQ_U32(out0->style.fg, 0u);
}

ZR_TEST_UNIT(drawlist_execute_fixture3_text_run_segments) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture3, zr_test_dl_fixture3_len, &lim, &v), ZR_OK);

  zr_fb_cell_t cells[8 * 1];
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, cells, 8u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim), ZR_OK);

  const zr_fb_cell_t* a = zr_fb_cell_at_const(&fb, 0u, 0u);
  const zr_fb_cell_t* d = zr_fb_cell_at_const(&fb, 3u, 0u);
  zr_assert_cell_glyph(ctx, a, (uint8_t)'A');
  zr_assert_cell_glyph(ctx, d, (uint8_t)'D');
  ZR_ASSERT_EQ_U32(a->style.fg, 1u);
  ZR_ASSERT_EQ_U32(d->style.fg, 3u);
}

