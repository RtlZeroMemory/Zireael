/*
  tests/unit/test_limits.c — Unit tests for util/zr_caps.h.

  Why: Validates that the default limits structure has sensible non-zero
  values and that validation rejects invalid configurations. Also verifies
  key runtime limit behavior that can be exercised in unit scope.

  Scenarios tested:
    - Default limits have all non-zero values and pass validation
    - Zero values for required fields cause validation failure
    - Invalid relationships (initial > max) cause validation failure
*/

#include "zr_test.h"

#include "core/zr_diff.h"
#include "core/zr_framebuffer.h"
#include "platform/zr_platform.h"
#include "util/zr_caps.h"

#include <string.h>

static void zr_limits_set_ascii(zr_fb_t* fb, uint32_t x, uint32_t y, uint8_t ch, zr_style_t style) {
  zr_cell_t* c = zr_fb_cell(fb, x, y);
  if (!c) {
    return;
  }
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = ch;
  c->glyph_len = 1u;
  c->width = 1u;
  c->style = style;
}

/*
 * Test: limits_default_and_validate
 *
 * Scenario: The default limits structure contains sensible non-zero values
 *           for all capacity fields and passes validation.
 *
 * Arrange: Obtain default limits.
 * Act:     Check all fields and call validate.
 * Assert:  All capacity fields are non-zero; validate returns ZR_OK.
 */
ZR_TEST_UNIT(limits_default_and_validate) {
  /* --- Arrange --- */
  zr_limits_t l = zr_limits_default();

  /* --- Assert: All capacity fields are non-zero --- */
  ZR_ASSERT_TRUE(l.arena_max_total_bytes != 0u);
  ZR_ASSERT_TRUE(l.arena_initial_bytes != 0u);
  ZR_ASSERT_TRUE(l.out_max_bytes_per_frame != 0u);
  ZR_ASSERT_TRUE(l.dl_max_total_bytes != 0u);
  ZR_ASSERT_TRUE(l.dl_max_cmds != 0u);
  ZR_ASSERT_TRUE(l.dl_max_strings != 0u);
  ZR_ASSERT_TRUE(l.dl_max_blobs != 0u);
  ZR_ASSERT_TRUE(l.dl_max_clip_depth != 0u);
  ZR_ASSERT_TRUE(l.dl_max_text_run_segments != 0u);
  ZR_ASSERT_TRUE(l.diff_max_damage_rects != 0u);

  /* --- Assert: Validation passes --- */
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_OK);
}

/*
 * Test: limits_validate_rejects_zero_or_invalid
 *
 * Scenario: Validation rejects limits structures with zero values for
 *           required fields or invalid relationships between fields.
 *
 * Arrange: Start with default limits, modify one field at a time.
 * Act:     Call validate with each invalid configuration.
 * Assert:  Each returns ZR_ERR_INVALID_ARGUMENT.
 */
ZR_TEST_UNIT(limits_validate_rejects_zero_or_invalid) {
  zr_limits_t l;

  /* --- Zero arena_max_total_bytes --- */
  l = zr_limits_default();
  l.arena_max_total_bytes = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  /* --- Zero arena_initial_bytes --- */
  l = zr_limits_default();
  l.arena_initial_bytes = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  /* --- Initial exceeds max (invalid relationship) --- */
  l = zr_limits_default();
  l.arena_initial_bytes = l.arena_max_total_bytes + 1u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  /* --- Zero dl_max_total_bytes --- */
  l = zr_limits_default();
  l.dl_max_total_bytes = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  /* --- Zero out_max_bytes_per_frame --- */
  l = zr_limits_default();
  l.out_max_bytes_per_frame = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  /* --- Zero diff_max_damage_rects --- */
  l = zr_limits_default();
  l.diff_max_damage_rects = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);
}

ZR_TEST_UNIT(limits_diff_max_damage_rects_forces_full_frame_when_cap_exceeded) {
  zr_fb_t prev = {0u, 0u, NULL};
  zr_fb_t next = {0u, 0u, NULL};
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 6u, 6u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 6u, 6u), ZR_OK);

  const zr_style_t s = {0u, 0u, 0u, 0u};
  (void)zr_fb_clear(&prev, &s);
  (void)zr_fb_clear(&next, &s);

  /* Two separated dirty spans on one row exceed a cap of 1 damage rect. */
  zr_limits_set_ascii(&next, 0u, 2u, (uint8_t)'A', s);
  zr_limits_set_ascii(&next, 2u, 2u, (uint8_t)'B', s);

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 1u;
  zr_damage_rect_t damage[1];

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.flags = ZR_TERM_STATE_VALID_ALL;
  initial.style = s;

  uint8_t out[2048];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render(&prev, &next, &caps, &initial, NULL, &lim, damage, 1u, 0u, out, sizeof(out),
                                        &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(stats.path_damage_used, 1u);
  ZR_ASSERT_EQ_U32(stats.damage_full_frame, 1u);
  ZR_ASSERT_EQ_U32(stats.damage_rects, 1u);
  ZR_ASSERT_EQ_U32(stats.damage_cells, 36u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}
