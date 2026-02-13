/*
  tests/unit/test_framebuffer_primitives.c — ASCII primitive drawing coverage.

  Why: Covers framebuffer primitive ops (lines, boxes, scrollbars) so clip-aware
  drawing behavior and delegation paths remain deterministic.
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

static void zr_assert_grid_row(zr_test_ctx_t* ctx, const zr_fb_t* fb, uint32_t y, const char* expected) {
  for (uint32_t x = 0u; x < fb->cols; x++) {
    ZR_ASSERT_EQ_U32(zr_cell_ch(fb, x, y), (uint8_t)expected[x]);
  }
}

ZR_TEST_UNIT(framebuffer_draw_hline_respects_clip_span) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 6u, 3u), ZR_OK);
  zr_fill_ascii(ctx, &fb, (uint8_t)'.');

  zr_rect_t stack[4];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 4u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clip_push(&p, (zr_rect_t){2, 1, 3, 1}), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_draw_hline(&p, 0, 1, 6, &s0), ZR_OK);

  zr_assert_grid_row(ctx, &fb, 0u, "......");
  zr_assert_grid_row(ctx, &fb, 1u, "..---.");
  zr_assert_grid_row(ctx, &fb, 2u, "......");

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(framebuffer_draw_vline_respects_clip_span) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 5u), ZR_OK);
  zr_fill_ascii(ctx, &fb, (uint8_t)'.');

  zr_rect_t stack[4];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 4u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clip_push(&p, (zr_rect_t){1, 2, 1, 2}), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_draw_vline(&p, 1, 0, 5, &s0), ZR_OK);

  zr_assert_grid_row(ctx, &fb, 0u, "....");
  zr_assert_grid_row(ctx, &fb, 1u, "....");
  zr_assert_grid_row(ctx, &fb, 2u, ".|..");
  zr_assert_grid_row(ctx, &fb, 3u, ".|..");
  zr_assert_grid_row(ctx, &fb, 4u, "....");

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(framebuffer_draw_box_renders_outline_chars) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 7u, 5u), ZR_OK);
  zr_fill_ascii(ctx, &fb, (uint8_t)'.');

  zr_rect_t stack[2];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_draw_box(&p, (zr_rect_t){1, 1, 5, 3}, &s0), ZR_OK);

  zr_assert_grid_row(ctx, &fb, 0u, ".......");
  zr_assert_grid_row(ctx, &fb, 1u, ".+---+.");
  zr_assert_grid_row(ctx, &fb, 2u, ".|...|.");
  zr_assert_grid_row(ctx, &fb, 3u, ".+---+.");
  zr_assert_grid_row(ctx, &fb, 4u, ".......");

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(framebuffer_draw_scrollbar_v_fills_track_and_thumb) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 5u), ZR_OK);
  zr_fill_ascii(ctx, &fb, (uint8_t)'.');

  zr_rect_t stack[2];
  zr_fb_painter_t p;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p, &fb, stack, 2u), ZR_OK);

  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_draw_scrollbar_v(&p, (zr_rect_t){1, 0, 1, 5}, (zr_rect_t){1, 2, 1, 2}, &s0, &s0), ZR_OK);

  zr_assert_grid_row(ctx, &fb, 0u, ". ..");
  zr_assert_grid_row(ctx, &fb, 1u, ". ..");
  zr_assert_grid_row(ctx, &fb, 2u, ".#..");
  zr_assert_grid_row(ctx, &fb, 3u, ".#..");
  zr_assert_grid_row(ctx, &fb, 4u, ". ..");

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(framebuffer_draw_scrollbar_h_matches_vertical_delegate) {
  zr_fb_t fb_v;
  zr_fb_t fb_h;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb_v, 6u, 3u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb_h, 6u, 3u), ZR_OK);
  zr_fill_ascii(ctx, &fb_v, (uint8_t)'.');
  zr_fill_ascii(ctx, &fb_h, (uint8_t)'.');

  zr_rect_t stack_v[2];
  zr_rect_t stack_h[2];
  zr_fb_painter_t p_v;
  zr_fb_painter_t p_h;
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p_v, &fb_v, stack_v, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_painter_begin(&p_h, &fb_h, stack_h, 2u), ZR_OK);

  const zr_style_t s0 = zr_style0();
  const zr_rect_t track = {0, 1, 6, 1};
  const zr_rect_t thumb = {2, 1, 2, 1};
  ZR_ASSERT_EQ_U32(zr_fb_draw_scrollbar_v(&p_v, track, thumb, &s0, &s0), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_draw_scrollbar_h(&p_h, track, thumb, &s0, &s0), ZR_OK);

  for (uint32_t y = 0u; y < fb_h.rows; y++) {
    for (uint32_t x = 0u; x < fb_h.cols; x++) {
      ZR_ASSERT_EQ_U32(zr_cell_ch(&fb_h, x, y), zr_cell_ch(&fb_v, x, y));
    }
  }
  zr_assert_grid_row(ctx, &fb_h, 0u, "......");
  zr_assert_grid_row(ctx, &fb_h, 1u, "  ##  ");
  zr_assert_grid_row(ctx, &fb_h, 2u, "......");

  zr_fb_release(&fb_v);
  zr_fb_release(&fb_h);
}
