/*
  tests/unit/test_cell_invariants.c — Wide-glyph and replacement invariants.

  Why: Ensures framebuffer writes never create half-wide glyphs and that the
  replacement policy (U+FFFD) triggers deterministically for oversized or
  out-of-bounds wide graphemes.
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

static void zr_painter_begin_1(zr_test_ctx_t* ctx, zr_fb_painter_t* p, zr_fb_t* fb, zr_rect_t* stack,
                               uint32_t cap) {
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(p, fb, stack, cap), ZR_OK);
}

ZR_TEST_UNIT(cell_invariant_wide_lead_has_continuation) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 3u, 1u), ZR_OK);
  const zr_style_t s0 = zr_style0();
  (void)zr_fb_clear(&fb, &s0);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  zr_painter_begin_1(ctx, &p, &fb, clip_stack, 2u);

  const uint8_t emoji[4] = {0xF0u, 0x9Fu, 0x99u, 0x82u};
  ZR_ASSERT_EQ_U32(zr_fb_put_grapheme(&p, 0, 0, emoji, sizeof(emoji), 2u, &s0), ZR_OK);

  const zr_cell_t* lead = zr_fb_cell_const(&fb, 0u, 0u);
  const zr_cell_t* cont = zr_fb_cell_const(&fb, 1u, 0u);
  ZR_ASSERT_TRUE(lead != NULL && cont != NULL);
  ZR_ASSERT_EQ_U32(lead->width, 2u);
  ZR_ASSERT_EQ_U32(lead->glyph_len, 4u);
  ZR_ASSERT_EQ_U32(cont->width, 0u);
  ZR_ASSERT_EQ_U32(cont->glyph_len, 0u);

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(cell_invariant_wide_at_line_end_renders_replacement_width1) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 3u, 1u), ZR_OK);
  const zr_style_t s0 = zr_style0();
  (void)zr_fb_clear(&fb, &s0);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  zr_painter_begin_1(ctx, &p, &fb, clip_stack, 2u);

  const uint8_t wide[] = {0xE7u, 0x95u, 0x8Cu}; /* U+754C '界' */
  ZR_ASSERT_EQ_U32(zr_fb_put_grapheme(&p, 2, 0, wide, sizeof(wide), 2u, &s0), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 2u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xEFu);
  ZR_ASSERT_EQ_U32(c->glyph[1], 0xBFu);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0xBDu);

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(cell_invariant_oversized_grapheme_renders_replacement) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 2u, 1u), ZR_OK);
  const zr_style_t s0 = zr_style0();
  (void)zr_fb_clear(&fb, &s0);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  zr_painter_begin_1(ctx, &p, &fb, clip_stack, 2u);

  uint8_t bytes[ZR_CELL_GLYPH_MAX + 1u];
  memset(bytes, (uint8_t)'A', sizeof(bytes));
  ZR_ASSERT_EQ_U32(zr_fb_put_grapheme(&p, 0, 0, bytes, sizeof(bytes), 1u, &s0), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xEFu);

  zr_fb_release(&fb);
}
