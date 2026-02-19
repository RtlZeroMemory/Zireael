/*
  tests/unit/test_diff_extended_styles.c — Diff renderer coverage for extended style features.

  Why: Locks deterministic byte output for underline variants, underline color,
  and OSC 8 hyperlink transitions (including capability-gated degradation).
*/

#include "zr_test.h"

#include "core/zr_diff.h"
#include "core/zr_framebuffer.h"
#include "platform/zr_platform.h"

#include <stdio.h>
#include <string.h>

enum {
  ZR_TEST_ATTR_UNDERLINE = 1u << 2u,
};

typedef struct zr_diff_case_result_t {
  zr_result_t rc;
  uint8_t out[8192];
  size_t out_len;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
} zr_diff_case_result_t;

static zr_style_t zr_style_default_ext(void) {
  zr_style_t s;
  s.fg_rgb = 0u;
  s.bg_rgb = 0u;
  s.attrs = 0u;
  s.reserved = 0u;
  s.underline_rgb = 0u;
  s.link_ref = 0u;
  return s;
}

static zr_fb_t zr_make_fb_1row_ext(uint32_t cols) {
  zr_fb_t fb;
  memset(&fb, 0, sizeof(fb));
  if (zr_fb_init(&fb, cols, 1u) != ZR_OK) {
    return fb;
  }
  const zr_style_t s = zr_style_default_ext();
  (void)zr_fb_clear(&fb, &s);
  return fb;
}

static void zr_set_cell_ascii_ext(zr_fb_t* fb, uint32_t x, uint8_t ch, zr_style_t style) {
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

static uint32_t zr_add_link(zr_fb_t* fb, const char* uri, const char* id) {
  uint32_t link_ref = 0u;
  const uint8_t* id_bytes = (id && id[0] != '\0') ? (const uint8_t*)id : NULL;
  const size_t id_len = (id_bytes != NULL) ? strlen(id) : 0u;
  if (zr_fb_link_intern(fb, (const uint8_t*)uri, strlen(uri), id_bytes, id_len, &link_ref) != ZR_OK) {
    return 0u;
  }
  return link_ref;
}

static plat_caps_t zr_caps_extended_all(void) {
  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;
  caps.supports_underline_styles = 1u;
  caps.supports_colored_underlines = 1u;
  caps.supports_hyperlinks = 1u;
  return caps;
}

static zr_diff_case_result_t zr_run_diff_case_with_flags(const zr_fb_t* prev, const zr_fb_t* next,
                                                         const plat_caps_t* caps, zr_style_t initial_style,
                                                         uint8_t initial_flags);

static zr_diff_case_result_t zr_run_diff_case(const zr_fb_t* prev, const zr_fb_t* next, const plat_caps_t* caps,
                                              zr_style_t initial_style) {
  return zr_run_diff_case_with_flags(prev, next, caps, initial_style, ZR_TERM_STATE_VALID_ALL);
}

static zr_diff_case_result_t zr_run_diff_case_with_flags(const zr_fb_t* prev, const zr_fb_t* next,
                                                         const plat_caps_t* caps, zr_style_t initial_style,
                                                         uint8_t initial_flags) {
  zr_diff_case_result_t out;
  memset(&out, 0, sizeof(out));

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.flags = initial_flags;
  initial.style = initial_style;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  out.rc = zr_diff_render(prev, next, caps, &initial, NULL, &lim, damage, 64u, 0u, out.out, sizeof(out.out),
                          &out.out_len, &out.final_state, &out.stats);
  return out;
}

static bool zr_bytes_contains(const uint8_t* hay, size_t hay_len, const uint8_t* needle, size_t needle_len) {
  if (!hay || !needle || needle_len == 0u || hay_len < needle_len) {
    return false;
  }
  for (size_t i = 0u; i + needle_len <= hay_len; i++) {
    if (memcmp(hay + i, needle, needle_len) == 0) {
      return true;
    }
  }
  return false;
}

static uint32_t zr_count_substr(const uint8_t* hay, size_t hay_len, const uint8_t* needle, size_t needle_len) {
  uint32_t n = 0u;
  if (!hay || !needle || needle_len == 0u || hay_len < needle_len) {
    return 0u;
  }
  for (size_t i = 0u; i + needle_len <= hay_len; i++) {
    if (memcmp(hay + i, needle, needle_len) == 0) {
      n++;
    }
  }
  return n;
}

ZR_TEST_UNIT(diff_underline_variants_emit_expected_sgr_forms) {
  for (uint32_t variant = 0u; variant <= 5u; variant++) {
    zr_fb_t prev = zr_make_fb_1row_ext(1u);
    zr_fb_t next = zr_make_fb_1row_ext(1u);

    zr_style_t s = zr_style_default_ext();
    s.attrs = ZR_TEST_ATTR_UNDERLINE;
    s.reserved = variant;
    zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

    const plat_caps_t caps = zr_caps_extended_all();
    const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
    ZR_ASSERT_EQ_U32(res.rc, ZR_OK);

    char expected[96];
    if (variant == 0u) {
      (void)snprintf(expected, sizeof(expected), "\x1b[0;4;38;2;0;0;0;48;2;0;0;0mX");
    } else {
      (void)snprintf(expected, sizeof(expected), "\x1b[0;4:%u;38;2;0;0;0;48;2;0;0;0mX", (unsigned)variant);
    }

    const size_t expected_len = strlen(expected);
    ZR_ASSERT_EQ_U32(res.out_len, (uint32_t)expected_len);
    ZR_ASSERT_MEMEQ(res.out, (const uint8_t*)expected, expected_len);

    zr_fb_release(&prev);
    zr_fb_release(&next);
  }
}

ZR_TEST_UNIT(diff_underline_style_cap_degrades_to_plain_underline) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t s = zr_style_default_ext();
  s.attrs = ZR_TEST_ATTR_UNDERLINE;
  s.reserved = 5u;
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  plat_caps_t caps = zr_caps_extended_all();
  caps.supports_underline_styles = 0u;

  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, (const uint8_t*)";4;", 3u));
  ZR_ASSERT_TRUE(!zr_bytes_contains(res.out, res.out_len, (const uint8_t*)"4:5", 3u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_colored_underline_emits_58_rgb) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t s = zr_style_default_ext();
  s.attrs = ZR_TEST_ATTR_UNDERLINE;
  s.reserved = 2u;
  s.underline_rgb = 0x00112233u;
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);

  const uint8_t expected[] = "\x1b[0;4:2;58;2;17;34;51;38;2;0;0;0;48;2;0;0;0mX";
  ZR_ASSERT_EQ_U32(res.out_len, (uint32_t)(sizeof(expected) - 1u));
  ZR_ASSERT_MEMEQ(res.out, expected, sizeof(expected) - 1u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_colored_underline_reset_emits_59) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t prev_style = zr_style_default_ext();
  prev_style.attrs = ZR_TEST_ATTR_UNDERLINE;
  prev_style.underline_rgb = 0x00335577u;
  zr_set_cell_ascii_ext(&prev, 0u, (uint8_t)'X', prev_style);

  zr_style_t next_style = prev_style;
  next_style.underline_rgb = 0u;
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', next_style);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, prev_style);
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, (const uint8_t*)";59;", 4u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_underline_style_and_color_order_is_stable) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t s = zr_style_default_ext();
  s.attrs = ZR_TEST_ATTR_UNDERLINE;
  s.reserved = 3u;
  s.underline_rgb = 0x00ABCDEFu;
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, (const uint8_t*)"4:3;58;2", 8u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_colored_underline_cap_omits_58) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t s = zr_style_default_ext();
  s.attrs = ZR_TEST_ATTR_UNDERLINE;
  s.underline_rgb = 0x00123456u;
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  plat_caps_t caps = zr_caps_extended_all();
  caps.supports_colored_underlines = 0u;

  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_TRUE(!zr_bytes_contains(res.out, res.out_len, (const uint8_t*)";58;", 4u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_style_change_detects_underline_variant_difference) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t prev_style = zr_style_default_ext();
  prev_style.attrs = ZR_TEST_ATTR_UNDERLINE;
  prev_style.reserved = 1u;
  zr_set_cell_ascii_ext(&prev, 0u, (uint8_t)'X', prev_style);

  zr_style_t next_style = prev_style;
  next_style.reserved = 3u;
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', next_style);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, prev_style);
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, (const uint8_t*)"4:3", 3u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_style_change_detects_underline_color_difference) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t prev_style = zr_style_default_ext();
  prev_style.attrs = ZR_TEST_ATTR_UNDERLINE;
  prev_style.underline_rgb = 0x00101010u;
  zr_set_cell_ascii_ext(&prev, 0u, (uint8_t)'X', prev_style);

  zr_style_t next_style = prev_style;
  next_style.underline_rgb = 0x00222222u;
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', next_style);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, prev_style);
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, (const uint8_t*)"58;2;34;34;34", 13u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_extended_zero_fields_match_v1_behavior) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t s = zr_style_default_ext();
  s.fg_rgb = 0x00112233u;
  s.attrs = 1u;
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);

  const uint8_t expected[] = "\x1b[0;1;38;2;17;34;51;48;2;0;0;0mX";
  ZR_ASSERT_EQ_U32(res.out_len, (uint32_t)(sizeof(expected) - 1u));
  ZR_ASSERT_MEMEQ(res.out, expected, sizeof(expected) - 1u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_open_close_emits_expected_osc8) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t s = zr_style_default_ext();
  s.link_ref = zr_add_link(&next, "https://example.com", NULL);
  ZR_ASSERT_TRUE(s.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);

  const uint8_t expected[] = "\x1b]8;;https://example.com\x1b\\X\x1b]8;;\x1b\\";
  ZR_ASSERT_EQ_U32(res.out_len, (uint32_t)(sizeof(expected) - 1u));
  ZR_ASSERT_MEMEQ(res.out, expected, sizeof(expected) - 1u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_open_with_id_emits_id_param) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t s = zr_style_default_ext();
  s.link_ref = zr_add_link(&next, "https://example.com/docs", "doc-42");
  ZR_ASSERT_TRUE(s.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  const char* expected_fragment = "]8;id=doc-42;https://example.com/docs";
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, (const uint8_t*)expected_fragment, strlen(expected_fragment)));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_transition_close_then_open) {
  zr_fb_t prev = zr_make_fb_1row_ext(2u);
  zr_fb_t next = zr_make_fb_1row_ext(2u);

  zr_style_t a = zr_style_default_ext();
  a.link_ref = zr_add_link(&next, "https://a.example", NULL);
  ZR_ASSERT_TRUE(a.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'A', a);

  zr_style_t b = zr_style_default_ext();
  b.link_ref = zr_add_link(&next, "https://b.example", NULL);
  ZR_ASSERT_TRUE(b.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 1u, (uint8_t)'B', b);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);

  const uint8_t needle[] = "\x1b]8;;\x1b\\\x1b]8;;https://b.example\x1b\\";
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, needle, sizeof(needle) - 1u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_transition_link_to_no_link_closes_only) {
  zr_fb_t prev = zr_make_fb_1row_ext(2u);
  zr_fb_t next = zr_make_fb_1row_ext(2u);

  zr_style_t linked = zr_style_default_ext();
  linked.link_ref = zr_add_link(&next, "https://close-only.example", NULL);
  ZR_ASSERT_TRUE(linked.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'A', linked);
  zr_set_cell_ascii_ext(&next, 1u, (uint8_t)'B', zr_style_default_ext());

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);

  const uint8_t close_then_b[] = "\x1b]8;;\x1b\\B";
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, close_then_b, sizeof(close_then_b) - 1u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_transition_no_link_to_link_opens_only) {
  zr_fb_t prev = zr_make_fb_1row_ext(2u);
  zr_fb_t next = zr_make_fb_1row_ext(2u);

  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'A', zr_style_default_ext());
  zr_style_t linked = zr_style_default_ext();
  linked.link_ref = zr_add_link(&next, "https://open-only.example", NULL);
  ZR_ASSERT_TRUE(linked.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 1u, (uint8_t)'B', linked);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);

  const uint8_t a_then_open[] = "A\x1b]8;;https://open-only.example\x1b\\B";
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, a_then_open, sizeof(a_then_open) - 1u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_same_link_across_cells_has_no_redundant_transitions) {
  zr_fb_t prev = zr_make_fb_1row_ext(2u);
  zr_fb_t next = zr_make_fb_1row_ext(2u);

  zr_style_t s = zr_style_default_ext();
  s.link_ref = zr_add_link(&next, "https://same.example", NULL);
  ZR_ASSERT_TRUE(s.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'A', s);
  zr_set_cell_ascii_ext(&next, 1u, (uint8_t)'B', s);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);

  const uint8_t osc_prefix[] = "\x1b]8;";
  ZR_ASSERT_EQ_U32(zr_count_substr(res.out, res.out_len, osc_prefix, sizeof(osc_prefix) - 1u), 2u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_capability_off_omits_osc8) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t s = zr_style_default_ext();
  s.link_ref = zr_add_link(&next, "https://example.com", NULL);
  ZR_ASSERT_TRUE(s.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  plat_caps_t caps = zr_caps_extended_all();
  caps.supports_hyperlinks = 0u;

  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_TRUE(!zr_bytes_contains(res.out, res.out_len, (const uint8_t*)"\x1b]8;", 4u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_uri_special_chars_are_emitted) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  const char* uri = "https://example.com/a?b=1&c=2#frag;semi";
  zr_style_t s = zr_style_default_ext();
  s.link_ref = zr_add_link(&next, uri, "id-1");
  ZR_ASSERT_TRUE(s.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, (const uint8_t*)uri, strlen(uri)));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_uri_length_limits_are_enforced) {
  zr_fb_t fb = zr_make_fb_1row_ext(1u);
  uint32_t ref = 0u;
  ZR_ASSERT_EQ_U32(zr_fb_link_intern(&fb, (const uint8_t*)"", 0u, NULL, 0u, &ref), ZR_ERR_LIMIT);

  char uri[ZR_FB_LINK_URI_MAX_BYTES + 1u];
  memset(uri, 'a', sizeof(uri));
  uri[sizeof(uri) - 1u] = '\0';
  ZR_ASSERT_EQ_U32(zr_fb_link_intern(&fb, (const uint8_t*)uri, ZR_FB_LINK_URI_MAX_BYTES, NULL, 0u, &ref), ZR_OK);
  ZR_ASSERT_TRUE(ref != 0u);

  zr_fb_release(&fb);
}

ZR_TEST_UNIT(diff_hyperlink_max_uri_length_emits_osc8) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  char uri[ZR_FB_LINK_URI_MAX_BYTES + 1u];
  memset(uri, 'a', sizeof(uri));
  uri[sizeof(uri) - 1u] = '\0';

  zr_style_t s = zr_style_default_ext();
  s.link_ref = zr_add_link(&next, uri, NULL);
  ZR_ASSERT_TRUE(s.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, (const uint8_t*)"\x1b]8;;", 5u));
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, (const uint8_t*)"\x1b]8;;\x1b\\", 7u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_state_does_not_leak_between_frames) {
  zr_fb_t blank = zr_make_fb_1row_ext(1u);
  zr_fb_t linked = zr_make_fb_1row_ext(1u);
  zr_fb_t unlinked = zr_make_fb_1row_ext(1u);

  zr_style_t link_style = zr_style_default_ext();
  link_style.link_ref = zr_add_link(&linked, "https://frame-one.example", NULL);
  ZR_ASSERT_TRUE(link_style.link_ref != 0u);
  zr_set_cell_ascii_ext(&linked, 0u, (uint8_t)'X', link_style);

  zr_style_t plain = zr_style_default_ext();
  zr_set_cell_ascii_ext(&unlinked, 0u, (uint8_t)'X', plain);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t first = zr_run_diff_case(&blank, &linked, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(first.rc, ZR_OK);
  ZR_ASSERT_EQ_U32(first.final_state.style.link_ref, 0u);

  const zr_diff_case_result_t second = zr_run_diff_case(&linked, &unlinked, &caps, first.final_state.style);
  ZR_ASSERT_EQ_U32(second.rc, ZR_OK);
  ZR_ASSERT_TRUE(!zr_bytes_contains(second.out, second.out_len, (const uint8_t*)"\x1b]8;", 4u));

  zr_fb_release(&blank);
  zr_fb_release(&linked);
  zr_fb_release(&unlinked);
}

ZR_TEST_UNIT(diff_hyperlink_style_unknown_still_emits_initial_sgr) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  zr_style_t s = zr_style_default_ext();
  s.link_ref = zr_add_link(&next, "https://unknown-style.example", NULL);
  ZR_ASSERT_TRUE(s.link_ref != 0u);
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', s);

  const plat_caps_t caps = zr_caps_extended_all();
  const uint8_t flags_without_style = (uint8_t)(ZR_TERM_STATE_CURSOR_POS_VALID | ZR_TERM_STATE_CURSOR_VIS_VALID |
                                                ZR_TERM_STATE_CURSOR_SHAPE_VALID | ZR_TERM_STATE_SCREEN_VALID);
  const zr_diff_case_result_t res =
      zr_run_diff_case_with_flags(&prev, &next, &caps, zr_style_default_ext(), flags_without_style);
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_TRUE(zr_bytes_contains(res.out, res.out_len, (const uint8_t*)"\x1b[0;38;2;0;0;0;48;2;0;0;0m", 26u));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hyperlink_equal_targets_with_different_refs_are_clean) {
  zr_fb_t prev = zr_make_fb_1row_ext(1u);
  zr_fb_t next = zr_make_fb_1row_ext(1u);

  const char* uri = "https://same-target.example";
  const char* id = "same-id";
  uint32_t prev_ref = 0u;
  uint32_t next_ref = 0u;
  ZR_ASSERT_EQ_U32(
      zr_fb_link_intern(&prev, (const uint8_t*)uri, strlen(uri), (const uint8_t*)id, strlen(id), &prev_ref), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_link_intern(&next, (const uint8_t*)"https://dummy.example", strlen("https://dummy.example"),
                                     NULL, 0u, &next_ref),
                   ZR_OK);
  ZR_ASSERT_TRUE(next_ref != 0u);
  ZR_ASSERT_EQ_U32(
      zr_fb_link_intern(&next, (const uint8_t*)uri, strlen(uri), (const uint8_t*)id, strlen(id), &next_ref), ZR_OK);

  zr_style_t prev_style = zr_style_default_ext();
  prev_style.link_ref = prev_ref;
  zr_set_cell_ascii_ext(&prev, 0u, (uint8_t)'X', prev_style);

  zr_style_t next_style = zr_style_default_ext();
  next_style.link_ref = next_ref;
  zr_set_cell_ascii_ext(&next, 0u, (uint8_t)'X', next_style);

  const plat_caps_t caps = zr_caps_extended_all();
  const zr_diff_case_result_t res = zr_run_diff_case(&prev, &next, &caps, zr_style_default_ext());
  ZR_ASSERT_EQ_U32(res.rc, ZR_OK);
  ZR_ASSERT_EQ_U32((uint32_t)res.out_len, 0u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}
