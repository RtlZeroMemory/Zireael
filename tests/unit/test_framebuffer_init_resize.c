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
  return s;
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
