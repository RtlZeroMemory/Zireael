/*
  tests/unit/test_cell_invariants.c — Wide-glyph and replacement invariants.

  Why: Ensures framebuffer writes never create half-wide glyphs and that the
  replacement policy (U+FFFD) triggers deterministically for oversized or
  out-of-bounds wide graphemes.

  Scenarios tested:
    - Wide glyph creates lead + continuation cell pair
    - Wide glyph at line end replaced with U+FFFD (no half-glyph)
    - Oversized grapheme (> ZR_CELL_GLYPH_MAX) replaced with U+FFFD
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
  s.underline_rgb = 0u;
  s.link_ref = 0u;
  return s;
}

static void zr_painter_begin_1(zr_test_ctx_t* ctx, zr_fb_painter_t* p, zr_fb_t* fb, zr_rect_t* stack, uint32_t cap) {
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(p, fb, stack, cap), ZR_OK);
}

/*
 * Test: cell_invariant_wide_lead_has_continuation
 *
 * Scenario: When a width-2 (wide) glyph is placed, it must occupy two cells:
 *           - Lead cell (width=2) contains the glyph bytes
 *           - Continuation cell (width=0) is empty (glyph_len=0)
 *
 * Arrange: Initialize 3x1 framebuffer, create painter.
 * Act:     Place a 4-byte emoji (U+1F642) at position (0,0) with width=2.
 * Assert:  Cell (0,0) is lead: width=2, glyph_len=4 (full emoji bytes).
 *          Cell (1,0) is continuation: width=0, glyph_len=0.
 */
ZR_TEST_UNIT(cell_invariant_wide_lead_has_continuation) {
  /* --- Arrange --- */
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 3u, 1u), ZR_OK);
  const zr_style_t s0 = zr_style0();
  (void)zr_fb_clear(&fb, &s0);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  zr_painter_begin_1(ctx, &p, &fb, clip_stack, 2u);

  /* U+1F642 "slightly smiling face" in UTF-8 */
  const uint8_t emoji[4] = {0xF0u, 0x9Fu, 0x99u, 0x82u};

  /* --- Act --- */
  ZR_ASSERT_EQ_U32(zr_fb_put_grapheme(&p, 0, 0, emoji, sizeof(emoji), 2u, &s0), ZR_OK);

  /* --- Assert --- */
  const zr_cell_t* lead = zr_fb_cell_const(&fb, 0u, 0u);
  const zr_cell_t* cont = zr_fb_cell_const(&fb, 1u, 0u);
  ZR_ASSERT_TRUE(lead != NULL && cont != NULL);

  /* Lead cell: contains glyph, width=2 */
  ZR_ASSERT_EQ_U32(lead->width, 2u);
  ZR_ASSERT_EQ_U32(lead->glyph_len, 4u);

  /* Continuation cell: empty, width=0 */
  ZR_ASSERT_EQ_U32(cont->width, 0u);
  ZR_ASSERT_EQ_U32(cont->glyph_len, 0u);

  /* --- Cleanup --- */
  zr_fb_release(&fb);
}

/*
 * Test: cell_invariant_wide_at_line_end_renders_replacement_width1
 *
 * Scenario: A wide glyph placed at the last column cannot fit (no room for
 *           continuation cell). The engine must replace it with U+FFFD (width=1)
 *           to avoid creating a half-glyph state.
 *
 * Arrange: Initialize 3x1 framebuffer (columns 0,1,2), create painter.
 * Act:     Place U+754C '界' (width=2) at column 2 (last column).
 * Assert:  Cell (2,0) contains U+FFFD (0xEF 0xBF 0xBD), width=1.
 *          (No continuation cell needed or created.)
 */
ZR_TEST_UNIT(cell_invariant_wide_at_line_end_renders_replacement_width1) {
  /* --- Arrange --- */
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 3u, 1u), ZR_OK);
  const zr_style_t s0 = zr_style0();
  (void)zr_fb_clear(&fb, &s0);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  zr_painter_begin_1(ctx, &p, &fb, clip_stack, 2u);

  /* U+754C '界' (CJK character, width=2) in UTF-8 */
  const uint8_t wide[] = {0xE7u, 0x95u, 0x8Cu};

  /* --- Act --- */
  /* Place at column 2 (last column) - no room for continuation. */
  ZR_ASSERT_EQ_U32(zr_fb_put_grapheme(&p, 2, 0, wide, sizeof(wide), 2u, &s0), ZR_OK);

  /* --- Assert --- */
  const zr_cell_t* c = zr_fb_cell_const(&fb, 2u, 0u);
  ZR_ASSERT_TRUE(c != NULL);

  /* Should be U+FFFD replacement character, width=1 (not wide). */
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xEFu); /* U+FFFD in UTF-8 */
  ZR_ASSERT_EQ_U32(c->glyph[1], 0xBFu);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0xBDu);

  /* --- Cleanup --- */
  zr_fb_release(&fb);
}

/*
 * Test: cell_invariant_oversized_grapheme_renders_replacement
 *
 * Scenario: A grapheme whose UTF-8 encoding exceeds ZR_CELL_GLYPH_MAX bytes
 *           cannot be stored. The engine must replace it with U+FFFD.
 *
 * Arrange: Initialize 2x1 framebuffer, create painter, prepare oversized buffer.
 * Act:     Attempt to place a grapheme with (ZR_CELL_GLYPH_MAX + 1) bytes.
 * Assert:  Cell (0,0) contains U+FFFD (0xEF 0xBF 0xBD), width=1.
 */
ZR_TEST_UNIT(cell_invariant_oversized_grapheme_renders_replacement) {
  /* --- Arrange --- */
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 2u, 1u), ZR_OK);
  const zr_style_t s0 = zr_style0();
  (void)zr_fb_clear(&fb, &s0);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  zr_painter_begin_1(ctx, &p, &fb, clip_stack, 2u);

  /* Create an oversized "grapheme" (exceeds ZR_CELL_GLYPH_MAX). */
  uint8_t bytes[ZR_CELL_GLYPH_MAX + 1u];
  memset(bytes, (uint8_t)'A', sizeof(bytes));

  /* --- Act --- */
  ZR_ASSERT_EQ_U32(zr_fb_put_grapheme(&p, 0, 0, bytes, sizeof(bytes), 1u, &s0), ZR_OK);

  /* --- Assert --- */
  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);

  /* Should be U+FFFD replacement character. */
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xEFu); /* U+FFFD in UTF-8 */

  /* --- Cleanup --- */
  zr_fb_release(&fb);
}

/*
 * Test: cell_invariant_empty_width1_grapheme_normalizes_to_space
 *
 * Scenario: An empty width-1 grapheme payload should not create a non-drawable
 *           width-1 cell. The framebuffer normalizes it to ASCII space.
 */
ZR_TEST_UNIT(cell_invariant_empty_width1_grapheme_normalizes_to_space) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 2u, 1u), ZR_OK);
  const zr_style_t s0 = zr_style0();
  (void)zr_fb_clear(&fb, &s0);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  zr_painter_begin_1(ctx, &p, &fb, clip_stack, 2u);

  ZR_ASSERT_EQ_U32(zr_fb_put_grapheme(&p, 0, 0, NULL, 0u, 1u, &s0), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], (uint8_t)' ');

  zr_fb_release(&fb);
}
