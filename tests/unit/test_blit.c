/*
  tests/unit/test_blit.c — BLIT_RECT overlap behavior and invariant preservation.

  Why: Ensures zr_fb_blit_rect behaves like memmove for overlaps and keeps
  wide-glyph continuation invariants valid after blits.
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

static void zr_set_ascii_cell(zr_test_ctx_t* ctx, zr_fb_t* fb, uint32_t x, uint8_t ch) {
  zr_cell_t* c = zr_fb_cell(fb, x, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = ch;
  c->glyph_len = 1u;
  c->width = 1u;
  c->style = zr_style0();
}

static void zr_assert_row_ascii(zr_test_ctx_t* ctx, const zr_fb_t* fb, const char* expected) {
  for (uint32_t x = 0u; x < fb->cols; x++) {
    const zr_cell_t* c = zr_fb_cell_const(fb, x, 0u);
    ZR_ASSERT_TRUE(c != NULL);
    ZR_ASSERT_EQ_U32(c->width, 1u);
    ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
    ZR_ASSERT_EQ_U32(c->glyph[0], (uint8_t)expected[x]);
  }
}

static void zr_assert_no_orphan_continuations(zr_test_ctx_t* ctx, const zr_fb_t* fb) {
  for (uint32_t y = 0u; y < fb->rows; y++) {
    for (uint32_t x = 0u; x < fb->cols; x++) {
      const zr_cell_t* c = zr_fb_cell_const(fb, x, y);
      ZR_ASSERT_TRUE(c != NULL);
      if (c->width == 0u) {
        ZR_ASSERT_TRUE(x > 0u);
        const zr_cell_t* lead = zr_fb_cell_const(fb, x - 1u, y);
        ZR_ASSERT_TRUE(lead != NULL);
        ZR_ASSERT_TRUE(lead->width == 2u);
      }
      if (c->width == 2u) {
        ZR_ASSERT_TRUE(x + 1u < fb->cols);
        const zr_cell_t* cont = zr_fb_cell_const(fb, x + 1u, y);
        ZR_ASSERT_TRUE(cont != NULL);
        ZR_ASSERT_TRUE(cont->width == 0u);
      }
    }
  }
}

ZR_TEST_UNIT(blit_overlap_right_shift_matches_memmove) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 6u, 1u), ZR_OK);

  const char* init = "ABCDEF";
  for (uint32_t i = 0u; i < 6u; i++) {
    zr_set_ascii_cell(ctx, &fb, i, (uint8_t)init[i]);
  }

  zr_rect_t stack[2];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  zr_rect_t src = {0, 0, 4, 1};
  zr_rect_t dst = {1, 0, 4, 1};
  ZR_ASSERT_EQ_U32(zr_fb_blit_rect(&p, dst, src), ZR_OK);

  zr_assert_row_ascii(ctx, &fb, "AABCDF");
  zr_assert_no_orphan_continuations(ctx, &fb);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(blit_overlap_left_shift_matches_memmove) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 6u, 1u), ZR_OK);

  const char* init = "ABCDEF";
  for (uint32_t i = 0u; i < 6u; i++) {
    zr_set_ascii_cell(ctx, &fb, i, (uint8_t)init[i]);
  }

  zr_rect_t stack[2];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  zr_rect_t src = {1, 0, 4, 1};
  zr_rect_t dst = {0, 0, 4, 1};
  ZR_ASSERT_EQ_U32(zr_fb_blit_rect(&p, dst, src), ZR_OK);

  zr_assert_row_ascii(ctx, &fb, "BCDEEF");
  zr_assert_no_orphan_continuations(ctx, &fb);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(blit_preserves_wide_glyph_invariants) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 1u), ZR_OK);
  const zr_style_t s0 = zr_style0();
  (void)zr_fb_clear(&fb, &s0);

  /* Seed: wide glyph at (0,0). */
  const uint8_t emoji[4] = {0xF0u, 0x9Fu, 0x99u, 0x82u};
  zr_cell_t* lead = zr_fb_cell(&fb, 0u, 0u);
  zr_cell_t* cont = zr_fb_cell(&fb, 1u, 0u);
  ZR_ASSERT_TRUE(lead != NULL && cont != NULL);
  memset(lead->glyph, 0, sizeof(lead->glyph));
  memcpy(lead->glyph, emoji, sizeof(emoji));
  lead->glyph_len = 4u;
  lead->width = 2u;
  lead->style = s0;
  memset(cont->glyph, 0, sizeof(cont->glyph));
  cont->glyph_len = 0u;
  cont->width = 0u;
  cont->style = s0;

  zr_rect_t stack[2];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  /* Non-overlapping blit of the wide glyph pair. */
  zr_rect_t src = {0, 0, 2, 1};
  zr_rect_t dst = {2, 0, 2, 1};
  ZR_ASSERT_EQ_U32(zr_fb_blit_rect(&p, dst, src), ZR_OK);

  const zr_cell_t* dlead = zr_fb_cell_const(&fb, 2u, 0u);
  const zr_cell_t* dcont = zr_fb_cell_const(&fb, 3u, 0u);
  ZR_ASSERT_TRUE(dlead != NULL && dcont != NULL);
  ZR_ASSERT_EQ_U32(dlead->width, 2u);
  ZR_ASSERT_EQ_U32(dlead->glyph_len, 4u);
  ZR_ASSERT_MEMEQ(dlead->glyph, emoji, 4u);
  ZR_ASSERT_EQ_U32(dcont->width, 0u);
  ZR_ASSERT_EQ_U32(dcont->glyph_len, 0u);

  zr_assert_no_orphan_continuations(ctx, &fb);
  zr_fb_release(&fb);
}
