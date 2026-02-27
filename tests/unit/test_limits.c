/*
  tests/unit/test_limits.c — Unit tests for util/zr_caps.h.

  Why: Validates that the default limits structure has sensible non-zero
  values and that validation rejects invalid configurations. Also verifies
  key runtime limit behavior that can be exercised in unit scope.

  Scenarios tested:
    - Default limits have all non-zero values and pass validation
    - Zero values for required fields cause validation failure
    - Invalid relationships (initial > max) cause validation failure
    - Clip-depth practical cap rejects >64 with no partial effects
*/

#include "zr_test.h"

#include "core/zr_diff.h"
#include "core/zr_drawlist.h"
#include "core/zr_framebuffer.h"
#include "platform/zr_platform.h"
#include "unicode/zr_width.h"
#include "util/zr_caps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fixtures defined in test_drawlist_validate.c */
extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;

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

ZR_TEST_UNIT(limits_validate_drawlist_caps_are_independent) {
  zr_limits_t l = zr_limits_default();
  l.dl_max_total_bytes = 1u;
  l.dl_max_cmds = 2u;
  l.dl_max_strings = 3u;
  l.dl_max_blobs = 4u;
  l.dl_max_clip_depth = 2u;
  l.dl_max_text_run_segments = 1u;
  l.diff_max_damage_rects = 1u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_OK);
}

ZR_TEST_UNIT(limits_execute_clip_depth_over_64_fails_without_partial_effects) {
  zr_limits_t validate_lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &validate_lim, &v), ZR_OK);

  zr_fb_t fb = {0};
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  const zr_style_t a = {0x01020304u, 0x11121314u, 0xA5A5A5A5u, 0u, 0u, 0u};
  const zr_style_t b = {0x21222324u, 0x31323334u, 0x5A5A5A5Au, 0u, 0u, 0u};
  zr_limits_set_ascii(&fb, 0u, 0u, (uint8_t)'X', a);
  zr_limits_set_ascii(&fb, 3u, 1u, (uint8_t)'Y', b);

  zr_cell_t before_cells[8];
  memcpy(before_cells, fb.cells, sizeof(before_cells));

  zr_cursor_state_t cursor = {0};
  zr_dl_resources_t resources;
  zr_dl_resources_init(&resources);
  cursor.x = 17;
  cursor.y = 23;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  cursor.visible = 1u;
  cursor.blink = 1u;
  cursor.reserved0 = 0u;
  zr_cursor_state_t before_cursor = cursor;

  zr_limits_t execute_lim = zr_limits_default();
  execute_lim.dl_max_clip_depth = 65u;

  const zr_result_t rc =
      zr_dl_execute(&v, &fb, &execute_lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, NULL, NULL, &resources, &cursor);
  ZR_ASSERT_EQ_U32(rc, ZR_ERR_LIMIT);
  ZR_ASSERT_TRUE(memcmp(before_cells, fb.cells, sizeof(before_cells)) == 0);
  ZR_ASSERT_TRUE(memcmp(&before_cursor, &cursor, sizeof(before_cursor)) == 0);

  zr_dl_resources_release(&resources);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(limits_diff_max_damage_rects_forces_full_frame_when_cap_exceeded) {
  zr_fb_t prev = {0};
  zr_fb_t next = {0};
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 6u, 6u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 6u, 6u), ZR_OK);

  const zr_style_t s = {0u, 0u, 0u, 0u, 0u, 0u};
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

ZR_TEST_UNIT(limits_link_intern_compacts_stale_refs_and_bounds_growth) {
  zr_fb_t fb;
  memset(&fb, 0, sizeof(fb));
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 2u, 1u), ZR_OK);

  const uint8_t persistent_uri[] = "https://example.test/persistent";
  uint32_t persistent_ref = 0u;
  ZR_ASSERT_EQ_U32(
      zr_fb_link_intern(&fb, persistent_uri, sizeof(persistent_uri) - 1u, NULL, 0u, &persistent_ref), ZR_OK);
  ZR_ASSERT_TRUE(persistent_ref != 0u);

  zr_cell_t* left = zr_fb_cell(&fb, 0u, 0u);
  zr_cell_t* right = zr_fb_cell(&fb, 1u, 0u);
  ZR_ASSERT_TRUE(left != NULL);
  ZR_ASSERT_TRUE(right != NULL);
  left->style.link_ref = persistent_ref;

  uint32_t peak_links_len = fb.links_len;
  uint32_t peak_link_bytes_len = fb.link_bytes_len;

  char uri_buf[96];
  for (uint32_t i = 0u; i < 64u; i++) {
    const int n = snprintf(uri_buf, sizeof(uri_buf), "https://example.test/ephemeral/%u", i);
    ZR_ASSERT_TRUE(n > 0 && (size_t)n < sizeof(uri_buf));

    uint32_t ref_i = 0u;
    ZR_ASSERT_EQ_U32(zr_fb_link_intern(&fb, (const uint8_t*)uri_buf, (size_t)n, NULL, 0u, &ref_i), ZR_OK);
    ZR_ASSERT_TRUE(ref_i >= 1u && ref_i <= fb.links_len);
    right->style.link_ref = ref_i;

    ZR_ASSERT_TRUE(left->style.link_ref >= 1u && left->style.link_ref <= fb.links_len);
    ZR_ASSERT_TRUE(right->style.link_ref >= 1u && right->style.link_ref <= fb.links_len);

    peak_links_len = ZR_MAX(peak_links_len, fb.links_len);
    peak_link_bytes_len = ZR_MAX(peak_link_bytes_len, fb.link_bytes_len);
  }

  ZR_ASSERT_TRUE(peak_links_len <= 5u);
  ZR_ASSERT_TRUE(peak_link_bytes_len <= (5u * (ZR_FB_LINK_URI_MAX_BYTES + ZR_FB_LINK_ID_MAX_BYTES)));

  const uint8_t* out_uri = NULL;
  size_t out_uri_len = 0u;
  const uint8_t* out_id = NULL;
  size_t out_id_len = 0u;
  ZR_ASSERT_EQ_U32(
      zr_fb_link_lookup(&fb, left->style.link_ref, &out_uri, &out_uri_len, &out_id, &out_id_len), ZR_OK);
  ZR_ASSERT_TRUE(out_uri != NULL);
  ZR_ASSERT_EQ_U32((uint32_t)out_uri_len, (uint32_t)(sizeof(persistent_uri) - 1u));
  ZR_ASSERT_TRUE(memcmp(out_uri, persistent_uri, out_uri_len) == 0);
  ZR_ASSERT_EQ_U32((uint32_t)out_id_len, 0u);
  ZR_ASSERT_TRUE(out_id == NULL);

  zr_fb_release(&fb);
}
