/*
  tests/unit/test_diff_spans.c — Unit coverage for diff span rules.

  Why: Validates span detection, wide-glyph continuation lead inclusion, and
  redundant CUP/SGR avoidance without relying on OS/terminal behavior.
*/

#include "zr_test.h"

#include "core/zr_diff.h"
#include "core/zr_framebuffer.h"
#include "platform/zr_platform.h"

#include <string.h>

static zr_fb_t zr_make_fb_1row(uint32_t cols) {
  zr_fb_t fb;
  (void)zr_fb_init(&fb, cols, 1u);
  zr_style_t s;
  s.fg_rgb = 0u;
  s.bg_rgb = 0u;
  s.attrs = 0u;
  s.reserved = 0u;
  (void)zr_fb_clear(&fb, &s);
  return fb;
}

static void zr_set_cell_ascii(zr_fb_t* fb, uint32_t x, uint8_t ch, zr_style_t style) {
  zr_cell_t* c = zr_fb_cell(fb, x, 0u);
  if (!c) {
    return;
  }
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = ch;
  c->glyph_len = 1u;
  c->width = 1u;
  c->style = style;
}

static void zr_set_cell_utf8(zr_fb_t* fb,
                             uint32_t x,
                             const uint8_t glyph[4],
                             uint8_t glyph_len,
                             uint8_t width,
                             zr_style_t style) {
  zr_cell_t* c = zr_fb_cell(fb, x, 0u);
  if (!c) {
    return;
  }
  memset(c->glyph, 0, sizeof(c->glyph));
  if (glyph_len != 0u) {
    memcpy(c->glyph, glyph, (size_t)glyph_len);
  }
  c->glyph_len = glyph_len;
  c->width = width;
  c->style = style;
}

ZR_TEST_UNIT(diff_span_separates_and_uses_cup) {
  zr_fb_t prev = zr_make_fb_1row(4u);
  zr_fb_t next = zr_make_fb_1row(4u);

  zr_style_t s = {0u, 0u, 0u, 0u};
  zr_set_cell_ascii(&next, 0u, (uint8_t)'A', s);
  zr_set_cell_ascii(&next, 2u, (uint8_t)'B', s);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.style = s;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[64];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render(&prev, &next, &caps, &initial, NULL, &lim, damage, 64u, 0u, out, sizeof(out),
                                       &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  const uint8_t expected[] = {
      (uint8_t)'A',
      0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'3', (uint8_t)'H',
      (uint8_t)'B',
  };
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_continuation_includes_lead) {
  zr_fb_t prev = zr_make_fb_1row(4u);
  zr_fb_t next = zr_make_fb_1row(4u);

  zr_style_t s = {0u, 0u, 0u, 0u};
  const uint8_t emoji[4] = {0xF0u, 0x9Fu, 0x99u, 0x82u};

  /* Lead is identical in prev/next; only the continuation cell differs. */
  zr_set_cell_utf8(&prev, 1u, emoji, 4u, 2u, s);
  zr_set_cell_utf8(&prev, 2u, (const uint8_t[4]){0u, 0u, 0u, 0u}, 0u, 0u, s);

  zr_set_cell_utf8(&next, 1u, emoji, 4u, 2u, s);
  zr_style_t s2 = s;
  s2.attrs = 1u;
  zr_set_cell_utf8(&next, 2u, (const uint8_t[4]){0u, 0u, 0u, 0u}, 0u, 0u, s2);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.style = s;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[64];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render(&prev, &next, &caps, &initial, NULL, &lim, damage, 64u, 0u, out, sizeof(out),
                                       &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  const uint8_t expected[] = {0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'2', (uint8_t)'H',
                              0xF0u, 0x9Fu, 0x99u, 0x82u};
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_avoids_redundant_cup_and_sgr) {
  zr_fb_t prev = zr_make_fb_1row(1u);
  zr_fb_t next = zr_make_fb_1row(1u);

  zr_style_t s;
  s.fg_rgb = 0x112233u;
  s.bg_rgb = 0x445566u;
  s.attrs = 1u; /* bold (v1) */
  s.reserved = 0u;

  zr_set_cell_ascii(&next, 0u, (uint8_t)'X', s);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.style = s;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[64];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render(&prev, &next, &caps, &initial, NULL, &lim, damage, 64u, 0u, out, sizeof(out),
                                       &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  const uint8_t expected[] = {(uint8_t)'X'};
  ZR_ASSERT_EQ_U32(out_len, 1u);
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_returns_limit_without_claiming_bytes) {
  zr_fb_t prev = zr_make_fb_1row(2u);
  zr_fb_t next = zr_make_fb_1row(2u);

  zr_style_t s = {0u, 0u, 0u, 0u};
  zr_set_cell_ascii(&next, 0u, (uint8_t)'H', s);
  zr_set_cell_ascii(&next, 1u, (uint8_t)'i', s);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.style = s;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[1];
  size_t out_len = 123u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render(&prev, &next, &caps, &initial, NULL, &lim, damage, 64u, 0u, out, sizeof(out),
                                       &out_len, &final_state, &stats);
  ZR_ASSERT_TRUE(rc == ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(out_len, 0u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}
