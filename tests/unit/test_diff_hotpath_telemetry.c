/*
  tests/unit/test_diff_hotpath_telemetry.c — Unit tests for diff hotpath telemetry.

  Why: Verifies that diff path-selection and hash-collision guard counters remain
  deterministic as hotpath optimizations evolve.
*/

#include "zr_test.h"

#include "core/zr_diff.h"
#include "core/zr_framebuffer.h"
#include "platform/zr_platform.h"

#include <string.h>

static void zr_set_cell_ascii(zr_fb_t* fb, uint32_t x, uint32_t y, uint8_t ch, zr_style_t style) {
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

static void zr_caps_default(plat_caps_t* caps) {
  if (!caps) {
    return;
  }
  memset(caps, 0, sizeof(*caps));
  caps->color_mode = PLAT_COLOR_MODE_RGB;
  caps->sgr_attrs_supported = 0xFFFFFFFFu;
}

static void zr_term_state_default(zr_term_state_t* ts, zr_style_t style) {
  if (!ts) {
    return;
  }
  memset(ts, 0, sizeof(*ts));
  ts->flags = ZR_TERM_STATE_VALID_ALL;
  ts->style = style;
}

ZR_TEST_UNIT(diff_telemetry_marks_damage_path_on_sparse_frame) {
  zr_fb_t prev = {0};
  zr_fb_t next = {0};
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 24u, 12u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 24u, 12u), ZR_OK);

  const zr_style_t s = {0u, 0u, 0u, 0u, 0u, 0u};
  (void)zr_fb_clear(&prev, &s);
  (void)zr_fb_clear(&next, &s);
  zr_set_cell_ascii(&next, 3u, 4u, (uint8_t)'X', s);

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 128u;
  zr_damage_rect_t damage[128];
  uint64_t prev_hashes[12];
  uint64_t next_hashes[12];
  uint8_t dirty_rows[12];
  memset(prev_hashes, 0, sizeof(prev_hashes));
  memset(next_hashes, 0, sizeof(next_hashes));
  memset(dirty_rows, 0, sizeof(dirty_rows));

  zr_diff_scratch_t scratch;
  memset(&scratch, 0, sizeof(scratch));
  scratch.prev_row_hashes = prev_hashes;
  scratch.next_row_hashes = next_hashes;
  scratch.dirty_rows = dirty_rows;
  scratch.row_cap = 12u;
  scratch.prev_hashes_valid = 0u;

  plat_caps_t caps;
  zr_caps_default(&caps);
  zr_term_state_t initial;
  zr_term_state_default(&initial, s);

  uint8_t out[4096];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render_ex(&prev, &next, &caps, &initial, NULL, &lim, damage, 128u, &scratch, 0u, out,
                                           sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(stats.path_damage_used, 1u);
  ZR_ASSERT_EQ_U32(stats.path_sweep_used, 0u);
  ZR_ASSERT_EQ_U32(stats.scroll_opt_attempted, 0u);
  ZR_ASSERT_EQ_U32(stats.scroll_opt_hit, 0u);
  ZR_ASSERT_EQ_U32(stats.collision_guard_hits, 0u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_telemetry_marks_sweep_path_on_dense_frame) {
  zr_fb_t prev = {0};
  zr_fb_t next = {0};
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 48u, 24u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 48u, 24u), ZR_OK);

  const zr_style_t s = {0u, 0u, 0u, 0u, 0u, 0u};
  (void)zr_fb_clear(&prev, &s);
  (void)zr_fb_clear(&next, &s);

  for (uint32_t y = 0u; y < 24u; y++) {
    for (uint32_t x = 0u; x < 48u; x++) {
      zr_set_cell_ascii(&prev, x, y, (uint8_t)('a' + ((x + y) % 26u)), s);
      zr_set_cell_ascii(&next, x, y, (uint8_t)('a' + ((x + y + 11u) % 26u)), s);
    }
  }

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 256u;
  zr_damage_rect_t damage[256];
  uint64_t prev_hashes[24];
  uint64_t next_hashes[24];
  uint8_t dirty_rows[24];
  memset(prev_hashes, 0, sizeof(prev_hashes));
  memset(next_hashes, 0, sizeof(next_hashes));
  memset(dirty_rows, 0, sizeof(dirty_rows));

  zr_diff_scratch_t scratch;
  memset(&scratch, 0, sizeof(scratch));
  scratch.prev_row_hashes = prev_hashes;
  scratch.next_row_hashes = next_hashes;
  scratch.dirty_rows = dirty_rows;
  scratch.row_cap = 24u;
  scratch.prev_hashes_valid = 0u;

  plat_caps_t caps;
  zr_caps_default(&caps);
  zr_term_state_t initial;
  zr_term_state_default(&initial, s);

  uint8_t out[32u * 1024u];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render_ex(&prev, &next, &caps, &initial, NULL, &lim, damage, 256u, &scratch, 0u, out,
                                           sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(stats.path_damage_used, 0u);
  ZR_ASSERT_EQ_U32(stats.path_sweep_used, 1u);
  ZR_ASSERT_EQ_U32(stats.collision_guard_hits, 0u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_telemetry_marks_scroll_attempt_and_hit) {
  zr_fb_t prev = {0};
  zr_fb_t next = {0};
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 80u, 12u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 80u, 12u), ZR_OK);

  const zr_style_t s = {0u, 0u, 0u, 0u, 0u, 0u};
  for (uint32_t y = 0u; y < 12u; y++) {
    const uint8_t ch = (uint8_t)('A' + (y % 26u));
    for (uint32_t x = 0u; x < 80u; x++) {
      zr_set_cell_ascii(&prev, x, y, ch, s);
    }
  }
  for (uint32_t y = 0u; y + 1u < 12u; y++) {
    const uint8_t ch = (uint8_t)('A' + ((y + 1u) % 26u));
    for (uint32_t x = 0u; x < 80u; x++) {
      zr_set_cell_ascii(&next, x, y, ch, s);
    }
  }
  for (uint32_t x = 0u; x < 80u; x++) {
    zr_set_cell_ascii(&next, x, 11u, (uint8_t)'#', s);
  }

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 256u;
  zr_damage_rect_t damage[256];
  uint64_t prev_hashes[12];
  uint64_t next_hashes[12];
  uint8_t dirty_rows[12];
  memset(prev_hashes, 0, sizeof(prev_hashes));
  memset(next_hashes, 0, sizeof(next_hashes));
  memset(dirty_rows, 0, sizeof(dirty_rows));

  zr_diff_scratch_t scratch;
  memset(&scratch, 0, sizeof(scratch));
  scratch.prev_row_hashes = prev_hashes;
  scratch.next_row_hashes = next_hashes;
  scratch.dirty_rows = dirty_rows;
  scratch.row_cap = 12u;
  scratch.prev_hashes_valid = 0u;

  plat_caps_t caps;
  zr_caps_default(&caps);
  caps.supports_scroll_region = 1u;
  zr_term_state_t initial;
  zr_term_state_default(&initial, s);

  uint8_t out[32u * 1024u];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render_ex(&prev, &next, &caps, &initial, NULL, &lim, damage, 256u, &scratch, 1u, out,
                                           sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(stats.scroll_opt_attempted, 1u);
  ZR_ASSERT_EQ_U32(stats.scroll_opt_hit, 1u);
  ZR_ASSERT_EQ_U32(stats.path_damage_used, 0u);
  ZR_ASSERT_EQ_U32(stats.path_sweep_used, 0u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_telemetry_counts_collision_guard_hits_with_reused_hashes) {
  zr_fb_t prev = {0};
  zr_fb_t next = {0};
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 32u, 10u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 32u, 10u), ZR_OK);

  const zr_style_t s = {0u, 0u, 0u, 0u, 0u, 0u};
  (void)zr_fb_clear(&prev, &s);
  (void)zr_fb_clear(&next, &s);
  zr_set_cell_ascii(&next, 5u, 1u, (uint8_t)'R', s);
  zr_set_cell_ascii(&next, 7u, 8u, (uint8_t)'S', s);

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 128u;
  zr_damage_rect_t damage[128];
  uint64_t prev_hashes[10];
  uint64_t next_hashes[10];
  uint8_t dirty_rows[10];
  memset(prev_hashes, 0, sizeof(prev_hashes));
  memset(next_hashes, 0, sizeof(next_hashes));
  memset(dirty_rows, 0, sizeof(dirty_rows));

  zr_diff_scratch_t scratch;
  memset(&scratch, 0, sizeof(scratch));
  scratch.prev_row_hashes = prev_hashes;
  scratch.next_row_hashes = next_hashes;
  scratch.dirty_rows = dirty_rows;
  scratch.row_cap = 10u;
  scratch.prev_hashes_valid = 0u;

  plat_caps_t caps;
  zr_caps_default(&caps);
  zr_term_state_t initial;
  zr_term_state_default(&initial, s);

  uint8_t out_a[4096];
  uint8_t out_b[4096];
  size_t out_a_len = 0u;
  size_t out_b_len = 0u;
  zr_term_state_t final_a;
  zr_term_state_t final_b;
  zr_diff_stats_t stats_a;
  zr_diff_stats_t stats_b;

  const zr_result_t rc_a = zr_diff_render_ex(&prev, &next, &caps, &initial, NULL, &lim, damage, 128u, &scratch, 0u,
                                             out_a, sizeof(out_a), &out_a_len, &final_a, &stats_a);
  ZR_ASSERT_EQ_U32(rc_a, ZR_OK);
  ZR_ASSERT_EQ_U32(stats_a.collision_guard_hits, 0u);

  memcpy(prev_hashes, next_hashes, sizeof(prev_hashes));
  scratch.prev_hashes_valid = 1u;

  const zr_result_t rc_b = zr_diff_render_ex(&prev, &next, &caps, &initial, NULL, &lim, damage, 128u, &scratch, 0u,
                                             out_b, sizeof(out_b), &out_b_len, &final_b, &stats_b);
  ZR_ASSERT_EQ_U32(rc_b, ZR_OK);
  ZR_ASSERT_EQ_U32(stats_b.collision_guard_hits, 2u);
  ZR_ASSERT_EQ_U32(stats_b.dirty_lines, 2u);
  ZR_ASSERT_EQ_U32((uint32_t)out_b_len, (uint32_t)out_a_len);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}
