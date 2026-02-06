/*
  tests/unit/test_drawlist_execute.c — Unit tests for drawlist v1 execution into zr_fb_t.

  Why: Validates that validated drawlists execute correctly, writing expected
  content to the framebuffer with proper styles, clipping, and text positioning.

  Scenarios tested:
    - Fixture 1: DRAW_TEXT writes correct glyphs and styles at expected positions
    - Fixture 2: Clipping is applied correctly to FILL_RECT
    - Fixture 3: DRAW_TEXT_RUN writes multiple segments with different styles
    - Fixture 4: Wide glyph clipping does not affect cursor advancement
*/

#include "zr_test.h"

#include "core/zr_drawlist.h"
#include "core/zr_framebuffer.h"
#include "unicode/zr_width.h"

/* Fixtures defined in test_drawlist_validate.c */
extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;
extern const uint8_t zr_test_dl_fixture2[];
extern const size_t zr_test_dl_fixture2_len;
extern const uint8_t zr_test_dl_fixture3[];
extern const size_t zr_test_dl_fixture3_len;
extern const uint8_t zr_test_dl_fixture4[];
extern const size_t zr_test_dl_fixture4_len;
extern const uint8_t zr_test_dl_fixture5_v2_cursor[];
extern const size_t zr_test_dl_fixture5_v2_cursor_len;
extern const uint8_t zr_test_dl_fixture6_v1_draw_text_slices[];
extern const size_t zr_test_dl_fixture6_v1_draw_text_slices_len;

/* Assert a cell contains a single ASCII byte with width=1. */
static void zr_assert_cell_glyph(zr_test_ctx_t* ctx, const zr_cell_t* c, uint8_t byte) {
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], byte);
  ZR_ASSERT_EQ_U32(c->width, 1u);
}

/*
 * Test: drawlist_execute_fixture1_text_written
 *
 * Scenario: Fixture 1 executes DRAW_TEXT("Hi") at position (1,0) with
 *           specified styles.
 *
 * Arrange: Validate fixture 1, create 4x2 framebuffer.
 * Act:     Execute drawlist.
 * Assert:  Cells (1,0) and (2,0) contain 'H' and 'i' with expected styles.
 */
ZR_TEST_UNIT(drawlist_execute_fixture1_text_written) {
  /* --- Arrange --- */
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &lim, &v), ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  /* --- Act --- */
  zr_cursor_state_t cursor = {0};
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  cursor.visible = 0u;
  cursor.blink = 0u;
  cursor.reserved0 = 0u;
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, &cursor), ZR_OK);

  /* --- Assert: Correct glyphs at expected positions --- */
  const zr_cell_t* c1 = zr_fb_cell_const(&fb, 1u, 0u);
  const zr_cell_t* c2 = zr_fb_cell_const(&fb, 2u, 0u);
  zr_assert_cell_glyph(ctx, c1, (uint8_t)'H');
  zr_assert_cell_glyph(ctx, c2, (uint8_t)'i');

  /* --- Assert: Styles match fixture values --- */
  ZR_ASSERT_EQ_U32(c1->style.fg_rgb, 0x01020304u);
  ZR_ASSERT_EQ_U32(c1->style.bg_rgb, 0x0A0B0C0Du);
  ZR_ASSERT_EQ_U32(c1->style.attrs, 0x00000011u);
  ZR_ASSERT_EQ_U32(c1->style.reserved, 0u);

  /* --- Cleanup --- */
  zr_fb_release(&fb);
}

/*
 * Test: drawlist_execute_fixture2_clip_applies
 *
 * Scenario: Fixture 2 clips FILL_RECT to region (1,1)-(3,2). Cells inside
 *           the clip have the filled style; cells outside remain unchanged.
 *
 * Arrange: Validate fixture 2, create 4x3 framebuffer.
 * Act:     Execute drawlist.
 * Assert:  Cells inside clip have fg=0x11111111; cells outside have fg=0.
 */
ZR_TEST_UNIT(drawlist_execute_fixture2_clip_applies) {
  /* --- Arrange --- */
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture2, zr_test_dl_fixture2_len, &lim, &v), ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 3u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  /* --- Act --- */
  zr_cursor_state_t cursor = {0};
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  cursor.visible = 0u;
  cursor.blink = 0u;
  cursor.reserved0 = 0u;
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, &cursor), ZR_OK);

  /* --- Assert: Cells inside clip region have filled style --- */
  const zr_cell_t* in0 = zr_fb_cell_const(&fb, 1u, 1u);
  const zr_cell_t* in1 = zr_fb_cell_const(&fb, 2u, 1u);
  ZR_ASSERT_EQ_U32(in0->style.fg_rgb, 0x11111111u);
  ZR_ASSERT_EQ_U32(in0->style.bg_rgb, 0x22222222u);
  ZR_ASSERT_EQ_U32(in1->style.fg_rgb, 0x11111111u);

  /* --- Assert: Cells outside clip region unchanged --- */
  const zr_cell_t* out0 = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_EQ_U32(out0->style.fg_rgb, 0u);

  /* --- Cleanup --- */
  zr_fb_release(&fb);
}

/*
 * Test: drawlist_execute_fixture3_text_run_segments
 *
 * Scenario: Fixture 3 uses DRAW_TEXT_RUN with two segments over "ABCDEF".
 *           Segment 0 (ABC) has fg=1; segment 1 (DEF) has fg=3.
 *
 * Arrange: Validate fixture 3, create 8x1 framebuffer.
 * Act:     Execute drawlist.
 * Assert:  Cell 0 ('A') has fg=1; cell 3 ('D') has fg=3.
 */
ZR_TEST_UNIT(drawlist_execute_fixture3_text_run_segments) {
  /* --- Arrange --- */
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture3, zr_test_dl_fixture3_len, &lim, &v), ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 8u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  /* --- Act --- */
  zr_cursor_state_t cursor = {0};
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  cursor.visible = 0u;
  cursor.blink = 0u;
  cursor.reserved0 = 0u;
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, &cursor), ZR_OK);

  /* --- Assert: Correct glyphs with segment-specific styles --- */
  const zr_cell_t* a = zr_fb_cell_const(&fb, 0u, 0u);
  const zr_cell_t* d = zr_fb_cell_const(&fb, 3u, 0u);
  zr_assert_cell_glyph(ctx, a, (uint8_t)'A');
  zr_assert_cell_glyph(ctx, d, (uint8_t)'D');
  ZR_ASSERT_EQ_U32(a->style.fg_rgb, 1u); /* Segment 0 style */
  ZR_ASSERT_EQ_U32(d->style.fg_rgb, 3u); /* Segment 1 style */

  /* --- Cleanup --- */
  zr_fb_release(&fb);
}

/*
 * Test: drawlist_execute_clip_does_not_change_wide_cursor_advance
 *
 * Scenario: Clipping must not affect cursor advancement for wide glyphs.
 *           Fixture 4 has clip at x==1 and draws "界A" (wide + ASCII) at x=0.
 *           The wide glyph should advance by 2, placing 'A' at x=2 (outside clip).
 *
 * Arrange: Validate fixture 4, create 4x1 framebuffer.
 * Act:     Execute drawlist.
 * Assert:  Cell x=1 (inside clip) is space (wide glyph clipped, 'A' advanced past).
 */
ZR_TEST_UNIT(drawlist_execute_clip_does_not_change_wide_cursor_advance) {
  /* --- Arrange --- */
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture4, zr_test_dl_fixture4_len, &lim, &v), ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  /* --- Act --- */
  zr_cursor_state_t cursor = {0};
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  cursor.visible = 0u;
  cursor.blink = 0u;
  cursor.reserved0 = 0u;
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, &cursor), ZR_OK);

  /*
   * The clip only includes x==1. The drawlist places a wide glyph starting at x==0
   * followed by 'A'. If wide-glyph advance were reduced to 1 due to clipping,
   * 'A' would be drawn into the visible cell x==1.
   */

  /* --- Assert: Cell x=1 is space (not 'A'), proving cursor advanced by 2 --- */
  const zr_cell_t* c = zr_fb_cell_const(&fb, 1u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], (uint8_t)' ');

  /* --- Cleanup --- */
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_execute_v2_set_cursor_updates_cursor_state) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture5_v2_cursor, zr_test_dl_fixture5_v2_cursor_len, &lim, &v), ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 2u, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  zr_cursor_state_t cursor = {0};
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  cursor.visible = 0u;
  cursor.blink = 0u;
  cursor.reserved0 = 0u;

  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, &cursor), ZR_OK);
  ZR_ASSERT_TRUE(cursor.x == 3);
  ZR_ASSERT_TRUE(cursor.y == 4);
  ZR_ASSERT_EQ_U32(cursor.shape, 2u);
  ZR_ASSERT_EQ_U32(cursor.visible, 1u);
  ZR_ASSERT_EQ_U32(cursor.blink, 1u);

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_execute_v1_draw_text_slices_share_string_bytes) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(
      zr_dl_validate(zr_test_dl_fixture6_v1_draw_text_slices, zr_test_dl_fixture6_v1_draw_text_slices_len, &lim, &v),
      ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 8u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  zr_cursor_state_t cursor = {0};
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  cursor.visible = 0u;
  cursor.blink = 0u;
  cursor.reserved0 = 0u;

  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, &cursor), ZR_OK);

  zr_assert_cell_glyph(ctx, zr_fb_cell_const(&fb, 0u, 0u), (uint8_t)'H');
  zr_assert_cell_glyph(ctx, zr_fb_cell_const(&fb, 1u, 0u), (uint8_t)'e');
  zr_assert_cell_glyph(ctx, zr_fb_cell_const(&fb, 2u, 0u), (uint8_t)'l');
  zr_assert_cell_glyph(ctx, zr_fb_cell_const(&fb, 3u, 0u), (uint8_t)'l');
  zr_assert_cell_glyph(ctx, zr_fb_cell_const(&fb, 4u, 0u), (uint8_t)'o');

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_execute_rejects_invalid_text_policy_arguments) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &lim, &v), ZR_OK);

  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  zr_cursor_state_t cursor = {0};
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  cursor.visible = 0u;
  cursor.blink = 0u;
  cursor.reserved0 = 0u;

  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim, 0u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, &cursor), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim, 4u, 999u, &cursor), ZR_ERR_INVALID_ARGUMENT);

  zr_fb_release(&fb);
}
