/*
  tests/golden/golden_diff_renderer.c — Golden tests for diff renderer bytes.

  Why: Ensures byte-for-byte stable VT/ANSI output for pinned caps + initial
  terminal state across representative fixtures.
*/

#include "zr_test.h"

#include "golden/zr_golden.h"

#include "core/zr_diff.h"
#include "core/zr_fb.h"
#include "platform/zr_platform.h"

#include <string.h>

static zr_style_t zr_style_default(void) {
  zr_style_t s;
  s.fg = 0u;
  s.bg = 0u;
  s.attrs = 0u;
  return s;
}

static zr_term_state_t zr_term_default(void) {
  zr_term_state_t t;
  memset(&t, 0, sizeof(t));
  t.cursor_x = 0u;
  t.cursor_y = 0u;
  t.style = zr_style_default();
  return t;
}

static plat_caps_t zr_caps_rgb_all_attrs(void) {
  plat_caps_t c;
  memset(&c, 0, sizeof(c));
  c.color_mode = PLAT_COLOR_MODE_RGB;
  c.sgr_attrs_supported = 0xFFFFFFFFu;
  return c;
}

static void zr_fb_set_ascii(zr_fb_t* fb, uint32_t x, uint32_t y, uint8_t ch, zr_style_t style) {
  zr_fb_cell_t* c = zr_fb_cell_at(fb, x, y);
  if (!c) {
    return;
  }
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = ch;
  c->glyph_len = 1u;
  c->flags = 0u;
  c->style = style;
}

static void zr_fb_set_utf8(zr_fb_t* fb,
                           uint32_t x,
                           uint32_t y,
                           const uint8_t glyph[4],
                           uint8_t glyph_len,
                           uint8_t flags,
                           zr_style_t style) {
  zr_fb_cell_t* c = zr_fb_cell_at(fb, x, y);
  if (!c) {
    return;
  }
  memset(c->glyph, 0, sizeof(c->glyph));
  if (glyph_len != 0u) {
    memcpy(c->glyph, glyph, (size_t)glyph_len);
  }
  c->glyph_len = glyph_len;
  c->flags = flags;
  c->style = style;
}

ZR_TEST_GOLDEN(diff_001_min_text_origin) {
  zr_fb_cell_t prev_cells[2];
  zr_fb_cell_t next_cells[2];
  zr_fb_t prev;
  zr_fb_t next;
  (void)zr_fb_init(&prev, prev_cells, 2u, 1u);
  (void)zr_fb_init(&next, next_cells, 2u, 1u);
  const zr_style_t s = zr_style_default();
  (void)zr_fb_clear(&prev, &s);
  (void)zr_fb_clear(&next, &s);
  zr_fb_set_ascii(&next, 0u, 0u, (uint8_t)'H', s);
  zr_fb_set_ascii(&next, 1u, 0u, (uint8_t)'i', s);

  const plat_caps_t caps = zr_caps_rgb_all_attrs();
  const zr_term_state_t initial = zr_term_default();

  uint8_t out[64];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc =
      zr_diff_render(&prev, &next, &caps, &initial, out, sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_TRUE(rc == ZR_OK);

  ZR_ASSERT_TRUE(zr_golden_compare_fixture("diff_001_min_text_origin", out, out_len) == 0);
}

ZR_TEST_GOLDEN(diff_002_style_change_single_glyph) {
  zr_fb_cell_t prev_cells[1];
  zr_fb_cell_t next_cells[1];
  zr_fb_t prev;
  zr_fb_t next;
  (void)zr_fb_init(&prev, prev_cells, 1u, 1u);
  (void)zr_fb_init(&next, next_cells, 1u, 1u);
  const zr_style_t s0 = zr_style_default();
  (void)zr_fb_clear(&prev, &s0);
  (void)zr_fb_clear(&next, &s0);

  zr_style_t s = s0;
  s.fg = 0xFF0000u;
  s.bg = 0x000000u;
  s.attrs = 1u; /* bold (v1) */
  zr_fb_set_ascii(&next, 0u, 0u, (uint8_t)'A', s);

  const plat_caps_t caps = zr_caps_rgb_all_attrs();
  const zr_term_state_t initial = zr_term_default();

  uint8_t out[128];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc =
      zr_diff_render(&prev, &next, &caps, &initial, out, sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_TRUE(rc == ZR_OK);

  ZR_ASSERT_TRUE(zr_golden_compare_fixture("diff_002_style_change_single_glyph", out, out_len) == 0);
}

ZR_TEST_GOLDEN(diff_003_wide_glyph_lead_only) {
  zr_fb_cell_t prev_cells[4];
  zr_fb_cell_t next_cells[4];
  zr_fb_t prev;
  zr_fb_t next;
  (void)zr_fb_init(&prev, prev_cells, 4u, 1u);
  (void)zr_fb_init(&next, next_cells, 4u, 1u);
  const zr_style_t s = zr_style_default();
  (void)zr_fb_clear(&prev, &s);
  (void)zr_fb_clear(&next, &s);

  const uint8_t emoji[4] = {0xF0u, 0x9Fu, 0x99u, 0x82u};
  zr_fb_set_utf8(&next, 1u, 0u, emoji, 4u, 0u, s);
  zr_fb_set_utf8(&next, 2u, 0u, (const uint8_t[4]){0u, 0u, 0u, 0u}, 0u, ZR_FB_CELL_FLAG_CONTINUATION, s);

  const plat_caps_t caps = zr_caps_rgb_all_attrs();
  const zr_term_state_t initial = zr_term_default();

  uint8_t out[128];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc =
      zr_diff_render(&prev, &next, &caps, &initial, out, sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_TRUE(rc == ZR_OK);

  ZR_ASSERT_TRUE(zr_golden_compare_fixture("diff_003_wide_glyph_lead_only", out, out_len) == 0);
}

