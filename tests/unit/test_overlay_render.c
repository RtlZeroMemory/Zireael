/*
  tests/unit/test_overlay_render.c — Debug overlay rendering bounds and invariants.

  Why: Ensures the debug overlay is deterministic, clipped to <=4x40, and does
  not break wide-glyph continuation invariants in the framebuffer.
*/

#include "zr_test.h"

#include "core/zr_debug_overlay.h"

#include <stdbool.h>
#include <string.h>

static void zr_fill_fb_ascii(zr_test_ctx_t* ctx, zr_fb_t* fb, uint8_t ch) {
  ZR_ASSERT_TRUE(fb != NULL);
  if (!fb->cells || fb->cols == 0u || fb->rows == 0u) {
    return;
  }
  const zr_style_t s = (zr_style_t){1u, 2u, 3u, 0u, 0u, 0u};
  for (uint32_t y = 0u; y < fb->rows; y++) {
    for (uint32_t x = 0u; x < fb->cols; x++) {
      zr_cell_t* c = zr_fb_cell(fb, x, y);
      ZR_ASSERT_TRUE(c != NULL);
      memset(c->glyph, 0, sizeof(c->glyph));
      c->glyph[0] = ch;
      c->glyph_len = 1u;
      c->width = 1u;
      c->style = s;
    }
  }
}

static void zr_assert_fb_continuations_valid(zr_test_ctx_t* ctx, const zr_fb_t* fb) {
  ZR_ASSERT_TRUE(fb != NULL);
  for (uint32_t y = 0u; y < fb->rows; y++) {
    for (uint32_t x = 0u; x < fb->cols; x++) {
      const zr_cell_t* c = zr_fb_cell_const(fb, x, y);
      ZR_ASSERT_TRUE(c != NULL);
      if (c->width == 0u) {
        ZR_ASSERT_TRUE(x > 0u);
        ZR_ASSERT_EQ_U32(c->glyph_len, 0u);
        const zr_cell_t* lead = zr_fb_cell_const(fb, x - 1u, y);
        ZR_ASSERT_TRUE(lead != NULL);
        ZR_ASSERT_TRUE(lead->width == 2u);
      }
    }
  }
}

static void zr_assert_overlay_cell(zr_test_ctx_t* ctx, const zr_fb_t* fb, uint32_t x, uint32_t y, uint8_t ch) {
  const zr_cell_t* c = zr_fb_cell_const(fb, x, y);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], ch);
  ZR_ASSERT_TRUE(c->width == 1u);
}

ZR_TEST_UNIT(overlay_renders_expected_ascii_within_4x40_region) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 50u, 6u), ZR_OK);
  zr_fill_fb_ascii(ctx, &fb, (uint8_t)'.');

  zr_metrics_t m;
  memset(&m, 0, sizeof(m));
  m.fps = 60u;
  m.bytes_emitted_last_frame = 1234u;
  m.dirty_lines_last_frame = 2u;
  m.dirty_cols_last_frame = 7u;
  m.us_input_last_frame = 1u;
  m.us_drawlist_last_frame = 2u;
  m.us_diff_last_frame = 3u;
  m.us_write_last_frame = 4u;
  m.events_out_last_poll = 5u;
  m.events_dropped_total = 6u;

  ZR_ASSERT_EQ_U32(zr_debug_overlay_render(&fb, &m), ZR_OK);

  const char* l0 = "FPS:60  BYTES:1234";
  const char* l1 = "DIRTY L:2 C:7";
  const char* l2 = "US in:1 dl:2 df:3 wr:4";
  const char* l3 = "EV out:5 drop:6";

  for (uint32_t y = 0u; y < fb.rows; y++) {
    for (uint32_t x = 0u; x < fb.cols; x++) {
      const bool in_overlay = (y < ZR_DEBUG_OVERLAY_MAX_ROWS) && (x < ZR_DEBUG_OVERLAY_MAX_COLS);
      if (!in_overlay) {
        zr_assert_overlay_cell(ctx, &fb, x, y, (uint8_t)'.');
      }
    }
  }

  /* Line 0. */
  for (uint32_t x = 0u; x < ZR_DEBUG_OVERLAY_MAX_COLS; x++) {
    const uint8_t ch = (x < (uint32_t)strlen(l0)) ? (uint8_t)l0[x] : (uint8_t)' ';
    zr_assert_overlay_cell(ctx, &fb, x, 0u, ch);
  }
  /* Line 1. */
  for (uint32_t x = 0u; x < ZR_DEBUG_OVERLAY_MAX_COLS; x++) {
    const uint8_t ch = (x < (uint32_t)strlen(l1)) ? (uint8_t)l1[x] : (uint8_t)' ';
    zr_assert_overlay_cell(ctx, &fb, x, 1u, ch);
  }
  /* Line 2. */
  for (uint32_t x = 0u; x < ZR_DEBUG_OVERLAY_MAX_COLS; x++) {
    const uint8_t ch = (x < (uint32_t)strlen(l2)) ? (uint8_t)l2[x] : (uint8_t)' ';
    zr_assert_overlay_cell(ctx, &fb, x, 2u, ch);
  }
  /* Line 3. */
  for (uint32_t x = 0u; x < ZR_DEBUG_OVERLAY_MAX_COLS; x++) {
    const uint8_t ch = (x < (uint32_t)strlen(l3)) ? (uint8_t)l3[x] : (uint8_t)' ';
    zr_assert_overlay_cell(ctx, &fb, x, 3u, ch);
  }

  zr_assert_fb_continuations_valid(ctx, &fb);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(overlay_clips_to_small_framebuffer) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 3u, 2u), ZR_OK);
  zr_fill_fb_ascii(ctx, &fb, (uint8_t)'.');

  zr_metrics_t m;
  memset(&m, 0, sizeof(m));
  m.fps = 60u;
  m.bytes_emitted_last_frame = 7u;

  ZR_ASSERT_EQ_U32(zr_debug_overlay_render(&fb, &m), ZR_OK);

  zr_assert_overlay_cell(ctx, &fb, 0u, 0u, (uint8_t)'F');
  zr_assert_overlay_cell(ctx, &fb, 1u, 0u, (uint8_t)'P');
  zr_assert_overlay_cell(ctx, &fb, 2u, 0u, (uint8_t)'S');

  zr_assert_overlay_cell(ctx, &fb, 0u, 1u, (uint8_t)'D');
  zr_assert_overlay_cell(ctx, &fb, 1u, 1u, (uint8_t)'I');
  zr_assert_overlay_cell(ctx, &fb, 2u, 1u, (uint8_t)'R');

  zr_assert_fb_continuations_valid(ctx, &fb);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(overlay_does_not_split_wide_glyph_across_right_edge) {
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 41u, 1u), ZR_OK);
  zr_fill_fb_ascii(ctx, &fb, (uint8_t)'.');

  const zr_style_t s = (zr_style_t){9u, 9u, 9u, 0u, 0u, 0u};
  const uint8_t wide[] = {0xE7u, 0x95u, 0x8Cu}; /* U+754C '界' */

  zr_cell_t* lead = zr_fb_cell(&fb, 39u, 0u);
  zr_cell_t* cont = zr_fb_cell(&fb, 40u, 0u);
  ZR_ASSERT_TRUE(lead != NULL && cont != NULL);
  memset(lead->glyph, 0, sizeof(lead->glyph));
  memcpy(lead->glyph, wide, sizeof(wide));
  lead->glyph_len = (uint8_t)sizeof(wide);
  lead->width = 2u;
  lead->style = s;
  memset(cont->glyph, 0, sizeof(cont->glyph));
  cont->glyph_len = 0u;
  cont->width = 0u;
  cont->style = s;

  const zr_cell_t* lead_before = zr_fb_cell_const(&fb, 39u, 0u);
  const zr_cell_t* cont_before = zr_fb_cell_const(&fb, 40u, 0u);
  ZR_ASSERT_TRUE(lead_before != NULL && cont_before != NULL);
  ZR_ASSERT_TRUE(cont_before->width == 0u);

  zr_metrics_t m;
  memset(&m, 0, sizeof(m));
  m.fps = 1u;
  m.bytes_emitted_last_frame = 1u;

  ZR_ASSERT_EQ_U32(zr_debug_overlay_render(&fb, &m), ZR_OK);

  const zr_cell_t* lead_after = zr_fb_cell_const(&fb, 39u, 0u);
  const zr_cell_t* cont_after = zr_fb_cell_const(&fb, 40u, 0u);
  ZR_ASSERT_TRUE(lead_after != NULL && cont_after != NULL);

  /* Cell 40 is outside overlay_cols; must not change. */
  ZR_ASSERT_TRUE(cont_after->width == 0u);
  ZR_ASSERT_EQ_U32(cont_after->glyph_len, 0u);

  /* Cell 39 is inside overlay; must not have been modified (would split glyph). */
  ZR_ASSERT_TRUE(lead_after->glyph_len == lead_before->glyph_len);
  ZR_ASSERT_MEMEQ(lead_after->glyph, lead_before->glyph, lead_before->glyph_len);

  zr_assert_fb_continuations_valid(ctx, &fb);
  zr_fb_release(&fb);
}
