/*
  tests/unit/test_framebuffer_text.c — Unicode-safe text drawing convenience.

  Why: Ensures zr_fb_draw_text_bytes() preserves wide-glyph invariants and
  applies the "no half glyph" replacement policy deterministically.
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

ZR_TEST_UNIT(framebuffer_draw_text_bytes_writes_ascii_cells) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 1u), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, &s0), ZR_OK);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, clip_stack, 2u), ZR_OK);

  const uint8_t bytes[] = {(uint8_t)'H', (uint8_t)'i'};
  ZR_ASSERT_EQ_U32(zr_fb_draw_text_bytes(&p, 0, 0, bytes, sizeof(bytes), &s0), ZR_OK);

  const zr_cell_t* c0 = zr_fb_cell_const(&fb, 0u, 0u);
  const zr_cell_t* c1 = zr_fb_cell_const(&fb, 1u, 0u);
  ZR_ASSERT_TRUE(c0 != NULL && c1 != NULL);
  ZR_ASSERT_EQ_U32(c0->width, 1u);
  ZR_ASSERT_EQ_U32(c0->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c0->glyph[0], (uint8_t)'H');
  ZR_ASSERT_EQ_U32(c1->width, 1u);
  ZR_ASSERT_EQ_U32(c1->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c1->glyph[0], (uint8_t)'i');

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(framebuffer_draw_text_bytes_wide_at_line_end_renders_replacement) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 1u), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, &s0), ZR_OK);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, clip_stack, 2u), ZR_OK);

  const uint8_t wide[] = {0xE7u, 0x95u, 0x8Cu}; /* U+754C '界' */
  ZR_ASSERT_EQ_U32(zr_fb_draw_text_bytes(&p, 3, 0, wide, sizeof(wide), &s0), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 3u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xEFu);
  ZR_ASSERT_EQ_U32(c->glyph[1], 0xBFu);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0xBDu);

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(framebuffer_draw_text_bytes_wide_clipped_renders_replacement_and_preserves_clip) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 1u), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, &s0), ZR_OK);

  zr_rect_t clip_stack[4];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, clip_stack, 4u), ZR_OK);

  /* Clip excludes x==2, so a wide glyph at x==1 can't fit fully. */
  ZR_ASSERT_EQ_U32(zr_fb_clip_push(&p, (zr_rect_t){0, 0, 2, 1}), ZR_OK);

  const uint8_t wide[] = {0xE7u, 0x95u, 0x8Cu}; /* U+754C '界' */
  const uint8_t seq[] = {wide[0], wide[1], wide[2], (uint8_t)'A'};

  ZR_ASSERT_EQ_U32(zr_fb_draw_text_bytes(&p, 1, 0, seq, sizeof(seq), &s0), ZR_OK);

  /* Cell 1 gets U+FFFD; clip ensures cell 2 is untouched (no half-glyph). */
  const zr_cell_t* c1 = zr_fb_cell_const(&fb, 1u, 0u);
  const zr_cell_t* c2 = zr_fb_cell_const(&fb, 2u, 0u);
  ZR_ASSERT_TRUE(c1 != NULL && c2 != NULL);
  ZR_ASSERT_EQ_U32(c1->width, 1u);
  ZR_ASSERT_EQ_U32(c1->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c1->glyph[0], 0xEFu);

  ZR_ASSERT_EQ_U32(c2->width, 1u);
  ZR_ASSERT_EQ_U32(c2->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c2->glyph[0], (uint8_t)' ');

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(framebuffer_draw_text_bytes_keycap_sequence_writes_wide_pair) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 1u), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, &s0), ZR_OK);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, clip_stack, 2u), ZR_OK);

  /* U+0031 U+FE0F U+20E3 ("1️⃣"). */
  const uint8_t keycap[] = {0x31u, 0xEFu, 0xB8u, 0x8Fu, 0xE2u, 0x83u, 0xA3u};
  ZR_ASSERT_EQ_U32(zr_fb_draw_text_bytes(&p, 1, 0, keycap, sizeof(keycap), &s0), ZR_OK);

  const zr_cell_t* lead = zr_fb_cell_const(&fb, 1u, 0u);
  const zr_cell_t* cont = zr_fb_cell_const(&fb, 2u, 0u);
  ZR_ASSERT_TRUE(lead != NULL && cont != NULL);
  ZR_ASSERT_EQ_U32(lead->width, 2u);
  ZR_ASSERT_EQ_U32(lead->glyph_len, (uint8_t)sizeof(keycap));
  ZR_ASSERT_MEMEQ(lead->glyph, keycap, sizeof(keycap));
  ZR_ASSERT_EQ_U32(cont->width, 0u);
  ZR_ASSERT_EQ_U32(cont->glyph_len, 0u);

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(framebuffer_put_grapheme_replaces_invalid_utf8_bytes) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, &s0), ZR_OK);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, clip_stack, 2u), ZR_OK);

  /* Standalone UTF-8 continuation byte: invalid in UTF-8 mode. */
  const uint8_t bad[] = {0x80u};
  ZR_ASSERT_EQ_U32(zr_fb_put_grapheme(&p, 0, 0, bad, sizeof(bad), 1u, &s0), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xEFu);
  ZR_ASSERT_EQ_U32(c->glyph[1], 0xBFu);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0xBDu);

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(framebuffer_put_grapheme_replaces_ascii_control_bytes) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, &s0), ZR_OK);

  zr_rect_t clip_stack[2];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, clip_stack, 2u), ZR_OK);

  /* U+001B ESC: printing raw ESC would corrupt the output stream. */
  const uint8_t esc = 0x1Bu;
  ZR_ASSERT_EQ_U32(zr_fb_put_grapheme(&p, 0, 0, &esc, 1u, 1u, &s0), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 3u);
  ZR_ASSERT_EQ_U32(c->glyph[0], 0xEFu);
  ZR_ASSERT_EQ_U32(c->glyph[1], 0xBFu);
  ZR_ASSERT_EQ_U32(c->glyph[2], 0xBDu);

  zr_fb_release(&fb);
}
