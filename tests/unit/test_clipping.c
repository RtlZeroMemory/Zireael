/*
  tests/unit/test_clipping.c — Painter clip stack intersections and no-op rects.

  Why: Ensures clip push/pop is deterministic and that empty rect ops do not
  mutate the framebuffer.
*/

#include "zr_test.h"

#include "core/zr_framebuffer.h"

#include <stdbool.h>
#include <string.h>

static zr_style_t zr_style0(void) {
  zr_style_t s;
  s.fg_rgb = 0u;
  s.bg_rgb = 0u;
  s.attrs = 0u;
  s.reserved = 0u;
  return s;
}

static void zr_fill_ascii(zr_test_ctx_t* ctx, zr_fb_t* fb, uint8_t ch) {
  ZR_ASSERT_TRUE(fb != NULL);
  for (uint32_t y = 0u; y < fb->rows; y++) {
    for (uint32_t x = 0u; x < fb->cols; x++) {
      zr_cell_t* c = zr_fb_cell(fb, x, y);
      ZR_ASSERT_TRUE(c != NULL);
      memset(c->glyph, 0, sizeof(c->glyph));
      c->glyph[0] = ch;
      c->glyph_len = 1u;
      c->width = 1u;
      c->style = zr_style0();
    }
  }
}

static uint8_t zr_cell_ch(const zr_fb_t* fb, uint32_t x, uint32_t y) {
  const zr_cell_t* c = zr_fb_cell_const(fb, x, y);
  if (!c || c->glyph_len == 0u) {
    return 0u;
  }
  return c->glyph[0];
}

ZR_TEST_UNIT(clipping_push_pop_intersections_apply_to_fill_rect) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 3u), ZR_OK);
  zr_fill_ascii(ctx, &fb, (uint8_t)'.');

  zr_rect_t stack[8];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 8u), ZR_OK);

  /* Clip A: cols 0..2, rows 0..2 */
  ZR_ASSERT_EQ_U32(zr_fb_clip_push(&p, (zr_rect_t){0, 0, 3, 3}), ZR_OK);
  /* Clip B: cols 1..3, rows 1..2 (bounded by fb) => intersection cols 1..2, rows 1..2 */
  ZR_ASSERT_EQ_U32(zr_fb_clip_push(&p, (zr_rect_t){1, 1, 3, 3}), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_fill_rect(&p, (zr_rect_t){0, 0, 4, 3}, &s0), ZR_OK);

  for (uint32_t y = 0u; y < fb.rows; y++) {
    for (uint32_t x = 0u; x < fb.cols; x++) {
      const bool in_intersection = (x >= 1u && x <= 2u) && (y >= 1u && y <= 2u);
      const uint8_t got = zr_cell_ch(&fb, x, y);
      if (in_intersection) {
        ZR_ASSERT_EQ_U32(got, (uint8_t)' ');
      } else {
        ZR_ASSERT_EQ_U32(got, (uint8_t)'.');
      }
    }
  }

  /* Pop B; clip is now A. Filling again affects A region. */
  ZR_ASSERT_EQ_U32(zr_fb_clip_pop(&p), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_fill_rect(&p, (zr_rect_t){0, 0, 4, 3}, &s0), ZR_OK);

  for (uint32_t y = 0u; y < fb.rows; y++) {
    for (uint32_t x = 0u; x < fb.cols; x++) {
      const bool in_a = (x <= 2u) && (y <= 2u);
      const uint8_t got = zr_cell_ch(&fb, x, y);
      if (in_a) {
        ZR_ASSERT_EQ_U32(got, (uint8_t)' ');
      } else {
        ZR_ASSERT_EQ_U32(got, (uint8_t)'.');
      }
    }
  }

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(clipping_rects_with_non_positive_size_are_noops_for_fill_rect) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 3u, 2u), ZR_OK);
  zr_fill_ascii(ctx, &fb, (uint8_t)'.');

  zr_rect_t stack[4];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 4u), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_fill_rect(&p, (zr_rect_t){0, 0, 0, 1}, &s0), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_fill_rect(&p, (zr_rect_t){0, 0, 1, 0}, &s0), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_fill_rect(&p, (zr_rect_t){0, 0, -1, 1}, &s0), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_fill_rect(&p, (zr_rect_t){0, 0, 1, -1}, &s0), ZR_OK);

  for (uint32_t y = 0u; y < fb.rows; y++) {
    for (uint32_t x = 0u; x < fb.cols; x++) {
      ZR_ASSERT_EQ_U32(zr_cell_ch(&fb, x, y), (uint8_t)'.');
    }
  }

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(clipping_wide_glyph_noop_when_lead_outside_clip) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 2u, 1u), ZR_OK);
  zr_fill_ascii(ctx, &fb, (uint8_t)'.');

  zr_rect_t stack[4];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 4u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clip_push(&p, (zr_rect_t){1, 0, 1, 1}), ZR_OK);

  const uint8_t wide = (uint8_t)'X';
  const zr_style_t style = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_put_grapheme(&p, 0, 0, &wide, 1u, 2u, &style), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_cell_ch(&fb, 0, 0), (uint8_t)'.');
  ZR_ASSERT_EQ_U32(zr_cell_ch(&fb, 1, 0), (uint8_t)'.');

  zr_fb_release(&fb);
}
