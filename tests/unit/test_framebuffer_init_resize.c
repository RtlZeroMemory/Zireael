/*
  tests/unit/test_framebuffer_init_resize.c — Framebuffer init/release/resize contracts.

  Why: Validates basic lifecycle behavior and the "no partial effects" guarantee
  for zr_fb_resize failure paths.

  Scenarios tested:
    - Basic init/release lifecycle
    - Resize failure preserves original state (no partial effects)
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

static void zr_fill_ascii(zr_test_ctx_t* ctx, zr_fb_t* fb, uint8_t ch) {
  ZR_ASSERT_TRUE(fb != NULL);
  const zr_style_t s = zr_style0();
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

static void zr_write_ascii_row(zr_test_ctx_t* ctx, zr_fb_t* fb, uint32_t y, const char* text) {
  ZR_ASSERT_TRUE(fb != NULL);
  ZR_ASSERT_TRUE(text != NULL);
  for (uint32_t x = 0u; x < fb->cols; x++) {
    zr_cell_t* c = zr_fb_cell(fb, x, y);
    ZR_ASSERT_TRUE(c != NULL);
    memset(c->glyph, 0, sizeof(c->glyph));
    c->glyph[0] = (uint8_t)text[x];
    c->glyph_len = 1u;
    c->width = 1u;
  }
}

static uint8_t zr_cell_ascii(zr_test_ctx_t* ctx, const zr_fb_t* fb, uint32_t x, uint32_t y) {
  (void)ctx;
  const zr_cell_t* c = zr_fb_cell_const(fb, x, y);
  if (!c) {
    return 0u;
  }
  if (c->glyph_len == 0u) {
    return 0u;
  }
  return c->glyph[0];
}

/*
 * Test: framebuffer_init_release_basics
 *
 * Scenario: Basic lifecycle - init creates backing store, release frees it.
 *
 * Arrange: Uninitialized framebuffer struct.
 * Act:     Call zr_fb_init() with 3x2 dimensions, then zr_fb_release().
 * Assert:  After init: cols/rows set, cells allocated.
 *          After release: cols/rows zeroed, cells NULL.
 */
ZR_TEST_UNIT(framebuffer_init_release_basics) {
  /* --- Arrange --- */
  zr_fb_t fb;

  /* --- Act: Init --- */
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 3u, 2u), ZR_OK);

  /* --- Assert: Init state --- */
  ZR_ASSERT_EQ_U32(fb.cols, 3u);
  ZR_ASSERT_EQ_U32(fb.rows, 2u);
  ZR_ASSERT_TRUE(fb.cells != NULL);

  /* --- Act: Release --- */
  zr_fb_release(&fb);

  /* --- Assert: Release state --- */
  ZR_ASSERT_EQ_U32(fb.cols, 0u);
  ZR_ASSERT_EQ_U32(fb.rows, 0u);
  ZR_ASSERT_TRUE(fb.cells == NULL);
}

/*
 * Test: framebuffer_resize_failure_has_no_partial_effects
 *
 * Scenario: When resize fails (e.g., due to excessive dimensions), the original
 *           framebuffer state must be preserved - no partial mutations.
 *
 * Arrange: Initialize 2x2 framebuffer, write 'X' to cell (0,0).
 * Act:     Attempt resize to impossibly large dimensions (0xFFFFFFFF, 1).
 * Assert:  Resize returns ZR_ERR_LIMIT.
 *          Original dimensions unchanged (2x2).
 *          Cell (0,0) still contains 'X' (data preserved).
 */
ZR_TEST_UNIT(framebuffer_resize_failure_has_no_partial_effects) {
  /* --- Arrange --- */
  zr_fb_t fb;
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 2u, 2u), ZR_OK);
  const zr_style_t s0 = zr_style0();
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, &s0), ZR_OK);

  /* Write 'X' to cell (0,0) as a marker. */
  zr_cell_t* c = zr_fb_cell(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = (uint8_t)'X';
  c->glyph_len = 1u;
  c->width = 1u;

  /* --- Act --- */
  /* Attempt resize to impossibly large dimensions (triggers ZR_ERR_LIMIT). */
  const zr_result_t rc = zr_fb_resize(&fb, 0xFFFFFFFFu, 1u);

  /* --- Assert --- */
  /* Resize should fail with limit error. */
  ZR_ASSERT_TRUE(rc == ZR_ERR_LIMIT);

  /* Original dimensions must be unchanged. */
  ZR_ASSERT_EQ_U32(fb.cols, 2u);
  ZR_ASSERT_EQ_U32(fb.rows, 2u);

  /* Original data must be preserved. */
  const zr_cell_t* c2 = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c2 != NULL);
  ZR_ASSERT_EQ_U32(c2->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c2->glyph[0], (uint8_t)'X');

  /* --- Cleanup --- */
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(framebuffer_copy_damage_rects_copies_clamped_inclusive_spans) {
  zr_fb_t src;
  zr_fb_t dst;
  ZR_ASSERT_EQ_U32(zr_fb_init(&src, 5u, 3u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&dst, 5u, 3u), ZR_OK);

  zr_fill_ascii(ctx, &src, (uint8_t)'?');
  zr_fill_ascii(ctx, &dst, (uint8_t)'.');
  zr_write_ascii_row(ctx, &src, 0u, "ABCDE");
  zr_write_ascii_row(ctx, &src, 1u, "FGHIJ");
  zr_write_ascii_row(ctx, &src, 2u, "KLMNO");

  const zr_damage_rect_t rects[] = {
      {1u, 0u, 3u, 1u},  /* middle block, two rows */
      {4u, 2u, 99u, 9u}, /* clamped to one bottom-right cell */
      {3u, 2u, 1u, 2u},  /* invalid (x0 > x1), ignored */
      {9u, 0u, 12u, 2u}, /* fully out of bounds, ignored */
  };

  ZR_ASSERT_EQ_U32(zr_fb_copy_damage_rects(&dst, &src, rects, (uint32_t)(sizeof(rects) / sizeof(rects[0]))), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 0u, 0u), (uint8_t)'.');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 1u, 0u), (uint8_t)'B');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 2u, 0u), (uint8_t)'C');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 3u, 0u), (uint8_t)'D');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 4u, 0u), (uint8_t)'.');

  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 0u, 1u), (uint8_t)'.');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 1u, 1u), (uint8_t)'G');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 2u, 1u), (uint8_t)'H');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 3u, 1u), (uint8_t)'I');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 4u, 1u), (uint8_t)'.');

  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 0u, 2u), (uint8_t)'.');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 1u, 2u), (uint8_t)'.');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 2u, 2u), (uint8_t)'.');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 3u, 2u), (uint8_t)'.');
  ZR_ASSERT_EQ_U32(zr_cell_ascii(ctx, &dst, 4u, 2u), (uint8_t)'O');

  zr_fb_release(&src);
  zr_fb_release(&dst);
}

ZR_TEST_UNIT(framebuffer_copy_damage_rects_rejects_dimension_mismatch) {
  zr_fb_t a;
  zr_fb_t b;
  ZR_ASSERT_EQ_U32(zr_fb_init(&a, 2u, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&b, 3u, 2u), ZR_OK);

  const zr_damage_rect_t r = {0u, 0u, 1u, 1u};
  ZR_ASSERT_EQ_U32(zr_fb_copy_damage_rects(&a, &b, &r, 1u), ZR_ERR_INVALID_ARGUMENT);

  zr_fb_release(&a);
  zr_fb_release(&b);
}
