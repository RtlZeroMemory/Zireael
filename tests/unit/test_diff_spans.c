/*
  tests/unit/test_diff_spans.c — Unit coverage for diff span rules.

  Why: Validates span detection, wide-glyph continuation lead inclusion, and
  redundant CUP/SGR avoidance plus SGR attr-mask behavior without relying on
  OS/terminal behavior.
*/

#include "zr_test.h"

#include "core/zr_diff.h"
#include "core/zr_framebuffer.h"
#include "platform/zr_platform.h"

#include <string.h>

enum {
  ZR_TEST_ATTR_BOLD = 1u << 0u,
  ZR_TEST_ATTR_ITALIC = 1u << 1u,
  ZR_TEST_ATTR_UNDERLINE = 1u << 2u,
  ZR_TEST_ATTR_REVERSE = 1u << 3u,
  ZR_TEST_ATTR_STRIKE = 1u << 4u,
};

typedef struct zr_diff_render_result_t {
  zr_result_t rc;
  uint8_t out[256];
  size_t out_len;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
} zr_diff_render_result_t;

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

static void zr_set_cell_utf8(zr_fb_t* fb, uint32_t x, const uint8_t glyph[4], uint8_t glyph_len, uint8_t width,
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

static uint32_t zr_count_byte(const uint8_t* bytes, size_t len, uint8_t needle) {
  if (!bytes) {
    return 0u;
  }
  uint32_t count = 0u;
  for (size_t i = 0u; i < len; i++) {
    if (bytes[i] == needle) {
      count++;
    }
  }
  return count;
}

static zr_diff_render_result_t zr_run_diff_render(const zr_fb_t* prev, const zr_fb_t* next, zr_style_t initial_style,
                                                  uint32_t sgr_attrs_supported) {
  zr_diff_render_result_t out;
  memset(&out, 0, sizeof(out));

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.sgr_attrs_supported = sgr_attrs_supported;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.flags = ZR_TERM_STATE_VALID_ALL;
  initial.style = initial_style;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  out.rc = zr_diff_render(prev, next, &caps, &initial, NULL, &lim, damage, 64u, 0u, out.out, sizeof(out.out),
                          &out.out_len, &out.final_state, &out.stats);
  return out;
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
  initial.flags = ZR_TERM_STATE_VALID_ALL;
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
      (uint8_t)'A', 0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'3', (uint8_t)'H', (uint8_t)'B',
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
  initial.flags = ZR_TERM_STATE_VALID_ALL;
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

  const uint8_t expected[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'2',
                              (uint8_t)'H', 0xF0u,        0x9Fu,        0x99u,        0x82u};
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
  initial.flags = ZR_TERM_STATE_VALID_ALL;
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

ZR_TEST_UNIT(diff_sgr_attr_clear_falls_back_to_reset) {
  zr_fb_t prev = zr_make_fb_1row(1u);
  zr_fb_t next = zr_make_fb_1row(1u);

  zr_style_t s_prev;
  s_prev.fg_rgb = 0x00AA0000u;
  s_prev.bg_rgb = 0x00000000u;
  s_prev.attrs = 1u;
  s_prev.reserved = 0u;

  zr_style_t s_next = s_prev;
  s_next.attrs = 0u;

  zr_set_cell_ascii(&prev, 0u, (uint8_t)'X', s_prev);
  zr_set_cell_ascii(&next, 0u, (uint8_t)'X', s_next);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.flags = ZR_TERM_STATE_VALID_ALL;
  initial.style = s_prev;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[128];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render(&prev, &next, &caps, &initial, NULL, &lim, damage, 64u, 0u, out, sizeof(out),
                                        &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  const uint8_t expected[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'0', (uint8_t)';', (uint8_t)'3', (uint8_t)'8', (uint8_t)';', (uint8_t)'2',
      (uint8_t)';', (uint8_t)'1', (uint8_t)'7', (uint8_t)'0', (uint8_t)';', (uint8_t)'0', (uint8_t)';', (uint8_t)'0',
      (uint8_t)';', (uint8_t)'4', (uint8_t)'8', (uint8_t)';', (uint8_t)'2', (uint8_t)';', (uint8_t)'0', (uint8_t)';',
      (uint8_t)'0', (uint8_t)';', (uint8_t)'0', (uint8_t)'m', (uint8_t)'X',
  };
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_sgr_attr_mask_per_attr_controls_emission) {
  typedef struct zr_attr_case_t {
    uint32_t bit;
    uint8_t sgr_digit;
  } zr_attr_case_t;

  static const zr_attr_case_t cases[] = {
      {ZR_TEST_ATTR_BOLD, (uint8_t)'1'},    {ZR_TEST_ATTR_ITALIC, (uint8_t)'3'}, {ZR_TEST_ATTR_UNDERLINE, (uint8_t)'4'},
      {ZR_TEST_ATTR_REVERSE, (uint8_t)'7'}, {ZR_TEST_ATTR_STRIKE, (uint8_t)'9'},
  };

  const zr_style_t base = {0u, 0u, 0u, 0u};
  for (size_t i = 0u; i < (sizeof(cases) / sizeof(cases[0])); i++) {
    zr_fb_t prev = zr_make_fb_1row(1u);
    zr_fb_t next = zr_make_fb_1row(1u);

    zr_style_t want = base;
    want.attrs = cases[i].bit;
    zr_set_cell_ascii(&prev, 0u, (uint8_t)'X', base);
    zr_set_cell_ascii(&next, 0u, (uint8_t)'X', want);

    const zr_diff_render_result_t with_support = zr_run_diff_render(&prev, &next, base, cases[i].bit);
    const uint8_t expected_with_support[] = {
        0x1Bu, (uint8_t)'[', cases[i].sgr_digit, (uint8_t)'m', (uint8_t)'X',
    };
    ZR_ASSERT_EQ_U32(with_support.rc, ZR_OK);
    ZR_ASSERT_EQ_U32(with_support.out_len, (uint32_t)sizeof(expected_with_support));
    ZR_ASSERT_MEMEQ(with_support.out, expected_with_support, sizeof(expected_with_support));
    ZR_ASSERT_EQ_U32(with_support.final_state.style.attrs, cases[i].bit);

    const zr_diff_render_result_t without_support = zr_run_diff_render(&prev, &next, base, 0u);
    const uint8_t expected_without_support[] = {(uint8_t)'X'};
    ZR_ASSERT_EQ_U32(without_support.rc, ZR_OK);
    ZR_ASSERT_EQ_U32(without_support.out_len, (uint32_t)sizeof(expected_without_support));
    ZR_ASSERT_MEMEQ(without_support.out, expected_without_support, sizeof(expected_without_support));
    ZR_ASSERT_EQ_U32(without_support.final_state.style.attrs, 0u);

    zr_fb_release(&prev);
    zr_fb_release(&next);
  }
}

ZR_TEST_UNIT(diff_sgr_attr_mask_mixed_add_subset_is_ordered) {
  zr_fb_t prev = zr_make_fb_1row(1u);
  zr_fb_t next = zr_make_fb_1row(1u);

  const zr_style_t base = {0u, 0u, 0u, 0u};
  zr_style_t want = base;
  want.attrs =
      ZR_TEST_ATTR_BOLD | ZR_TEST_ATTR_ITALIC | ZR_TEST_ATTR_UNDERLINE | ZR_TEST_ATTR_REVERSE | ZR_TEST_ATTR_STRIKE;
  zr_set_cell_ascii(&prev, 0u, (uint8_t)'X', base);
  zr_set_cell_ascii(&next, 0u, (uint8_t)'X', want);

  const uint32_t supported = ZR_TEST_ATTR_BOLD | ZR_TEST_ATTR_UNDERLINE | ZR_TEST_ATTR_STRIKE;
  const zr_diff_render_result_t res = zr_run_diff_render(&prev, &next, base, supported);

  const uint8_t expected[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'4',
      (uint8_t)';', (uint8_t)'9', (uint8_t)'m', (uint8_t)'X',
  };
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_EQ_U32(res.out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(res.out, expected, sizeof(expected));
  ZR_ASSERT_EQ_U32(res.final_state.style.attrs, supported);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_sgr_attr_mask_mixed_reset_then_add_transitions) {
  zr_fb_t prev = zr_make_fb_1row(3u);
  zr_fb_t next = zr_make_fb_1row(3u);

  const zr_style_t base = {0u, 0u, 0u, 0u};
  zr_set_cell_ascii(&prev, 0u, (uint8_t)'A', base);
  zr_set_cell_ascii(&prev, 1u, (uint8_t)'B', base);
  zr_set_cell_ascii(&prev, 2u, (uint8_t)'C', base);

  zr_style_t s0 = base;
  s0.attrs = ZR_TEST_ATTR_BOLD | ZR_TEST_ATTR_ITALIC | ZR_TEST_ATTR_UNDERLINE | ZR_TEST_ATTR_STRIKE;
  zr_style_t s1 = base;
  s1.attrs = ZR_TEST_ATTR_ITALIC | ZR_TEST_ATTR_STRIKE;
  zr_style_t s2 = base;
  s2.attrs = ZR_TEST_ATTR_REVERSE | ZR_TEST_ATTR_ITALIC;
  zr_set_cell_ascii(&next, 0u, (uint8_t)'A', s0);
  zr_set_cell_ascii(&next, 1u, (uint8_t)'B', s1);
  zr_set_cell_ascii(&next, 2u, (uint8_t)'C', s2);

  const uint32_t supported = ZR_TEST_ATTR_BOLD | ZR_TEST_ATTR_UNDERLINE | ZR_TEST_ATTR_REVERSE;
  const zr_diff_render_result_t res = zr_run_diff_render(&prev, &next, base, supported);

  const uint8_t expected[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'4', (uint8_t)'m', (uint8_t)'A', 0x1Bu,
      (uint8_t)'[', (uint8_t)'0', (uint8_t)';', (uint8_t)'3', (uint8_t)'8', (uint8_t)';', (uint8_t)'2', (uint8_t)';',
      (uint8_t)'0', (uint8_t)';', (uint8_t)'0', (uint8_t)';', (uint8_t)'0', (uint8_t)';', (uint8_t)'4', (uint8_t)'8',
      (uint8_t)';', (uint8_t)'2', (uint8_t)';', (uint8_t)'0', (uint8_t)';', (uint8_t)'0', (uint8_t)';', (uint8_t)'0',
      (uint8_t)'m', (uint8_t)'B', 0x1Bu,        (uint8_t)'[', (uint8_t)'7', (uint8_t)'m', (uint8_t)'C',
  };
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_EQ_U32(res.out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(res.out, expected, sizeof(expected));
  ZR_ASSERT_EQ_U32(res.final_state.style.attrs, ZR_TEST_ATTR_REVERSE);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_sgr_attr_mask_ignores_masked_attr_clear_between_cells) {
  zr_fb_t prev = zr_make_fb_1row(2u);
  zr_fb_t next = zr_make_fb_1row(2u);

  const zr_style_t base = {0u, 0u, 0u, 0u};
  zr_set_cell_ascii(&prev, 0u, (uint8_t)'A', base);
  zr_set_cell_ascii(&prev, 1u, (uint8_t)'B', base);

  zr_style_t s0 = base;
  s0.attrs = ZR_TEST_ATTR_BOLD | ZR_TEST_ATTR_ITALIC;
  zr_style_t s1 = base;
  s1.attrs = ZR_TEST_ATTR_BOLD;
  zr_set_cell_ascii(&next, 0u, (uint8_t)'A', s0);
  zr_set_cell_ascii(&next, 1u, (uint8_t)'B', s1);

  const zr_diff_render_result_t res = zr_run_diff_render(&prev, &next, base, ZR_TEST_ATTR_BOLD);
  const uint8_t expected[] = {0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)'m', (uint8_t)'A', (uint8_t)'B'};
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_EQ_U32(res.out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(res.out, expected, sizeof(expected));
  ZR_ASSERT_EQ_U32(res.final_state.style.attrs, ZR_TEST_ATTR_BOLD);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_damage_coalescing_keeps_unsorted_spans) {
  zr_fb_t prev;
  zr_fb_t next;
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 64u, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 64u, 2u), ZR_OK);

  zr_style_t s = {0u, 0u, 0u, 0u};
  (void)zr_fb_clear(&prev, &s);
  (void)zr_fb_clear(&next, &s);

  zr_cell_t* c = zr_fb_cell(&next, 50u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = (uint8_t)'A';
  c->glyph_len = 1u;
  c->width = 1u;
  c->style = s;

  c = zr_fb_cell(&next, 50u, 1u);
  ZR_ASSERT_TRUE(c != NULL);
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = (uint8_t)'A';
  c->glyph_len = 1u;
  c->width = 1u;
  c->style = s;

  c = zr_fb_cell(&next, 10u, 1u);
  ZR_ASSERT_TRUE(c != NULL);
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = (uint8_t)'B';
  c->glyph_len = 1u;
  c->width = 1u;
  c->style = s;

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.flags = ZR_TERM_STATE_VALID_ALL;
  initial.style = s;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[256];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render(&prev, &next, &caps, &initial, NULL, &lim, damage, 64u, 0u, out, sizeof(out),
                                        &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  ZR_ASSERT_EQ_U32(zr_count_byte(out, out_len, (uint8_t)'A'), 2u);
  ZR_ASSERT_EQ_U32(zr_count_byte(out, out_len, (uint8_t)'B'), 1u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_reserved_only_style_change_emits_complete_stream) {
  zr_fb_t prev = zr_make_fb_1row(1u);
  zr_fb_t next = zr_make_fb_1row(1u);

  zr_style_t s_prev = {0x00112233u, 0x00000000u, 0u, 0u};
  zr_style_t s_next = s_prev;
  s_next.reserved = 1u;

  zr_set_cell_ascii(&prev, 0u, (uint8_t)'X', s_prev);
  zr_set_cell_ascii(&next, 0u, (uint8_t)'X', s_next);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.flags = ZR_TERM_STATE_VALID_ALL;
  initial.style = s_prev;

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
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
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
  initial.flags = ZR_TERM_STATE_VALID_ALL;
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
