/*
  tests/golden/golden_diff_renderer.c — Golden tests for diff renderer bytes.

  Why: Ensures byte-for-byte stable VT/ANSI output for pinned caps + initial
  terminal state across representative fixtures.
*/

#include "zr_test.h"

#include "golden/zr_golden.h"

#include "core/zr_diff.h"
#include "core/zr_framebuffer.h"
#include "platform/zr_platform.h"

#include <string.h>

static zr_style_t zr_style_default(void) {
  zr_style_t s;
  s.fg_rgb = 0u;
  s.bg_rgb = 0u;
  s.attrs = 0u;
  s.reserved = 0u;
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

/* Set a cell to a UTF-8 grapheme with specified width (for wide glyphs). */
static void zr_fb_set_utf8(zr_fb_t* fb,
                           uint32_t x,
                           uint32_t y,
                           const uint8_t glyph[4],
                           uint8_t glyph_len,
                           uint8_t width,
                           zr_style_t style) {
  zr_cell_t* c = zr_fb_cell(fb, x, y);
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

static void zr_fb_fill_row_ascii(zr_fb_t* fb, uint32_t y, uint8_t ch, zr_style_t style) {
  if (!fb) {
    return;
  }
  for (uint32_t x = 0u; x < fb->cols; x++) {
    zr_fb_set_ascii(fb, x, y, ch, style);
  }
}

/*
 * Test: diff_001_min_text_origin
 *
 * Scenario: Minimal diff with two ASCII characters at origin (0,0).
 *           Cursor starts at origin, so no CUP needed; just emit "Hi".
 *
 * Arrange: 2x1 framebuffer, prev=clear, next="Hi".
 * Act:     Render diff.
 * Assert:  Output matches pinned golden fixture.
 */
ZR_TEST_GOLDEN(diff_001_min_text_origin) {
  zr_fb_t prev;
  zr_fb_t next;
  (void)zr_fb_init(&prev, 2u, 1u);
  (void)zr_fb_init(&next, 2u, 1u);
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
      zr_diff_render(&prev, &next, &caps, &initial, 0u, out, sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_TRUE(rc == ZR_OK);

  ZR_ASSERT_TRUE(zr_golden_compare_fixture("diff_001_min_text_origin", out, out_len) == 0);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

/*
 * Test: diff_004_scroll_region_scroll_up_fullscreen
 *
 * Scenario: Fullscreen scroll-up by 1 line where most rows are identical after the shift.
 *           When scroll optimizations are enabled and supported, emit DECSTBM + SU and
 *           redraw only the newly exposed bottom line.
 */
ZR_TEST_GOLDEN(diff_004_scroll_region_scroll_up_fullscreen) {
  zr_fb_t prev;
  zr_fb_t next;
  (void)zr_fb_init(&prev, 16u, 17u);
  (void)zr_fb_init(&next, 16u, 17u);
  const zr_style_t s = zr_style_default();
  (void)zr_fb_clear(&prev, &s);
  (void)zr_fb_clear(&next, &s);

  for (uint32_t y = 0u; y < 17u; y++) {
    const uint8_t ch = (uint8_t)('A' + (uint8_t)y);
    zr_fb_fill_row_ascii(&prev, y, ch, s);
  }
  for (uint32_t y = 0u; y < 16u; y++) {
    const uint8_t ch = (uint8_t)('B' + (uint8_t)y);
    zr_fb_fill_row_ascii(&next, y, ch, s);
  }
  zr_fb_fill_row_ascii(&next, 16u, (uint8_t)'R', s);

  plat_caps_t caps = zr_caps_rgb_all_attrs();
  caps.supports_scroll_region = 1u;
  const zr_term_state_t initial = zr_term_default();

  uint8_t out[256];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc =
      zr_diff_render(&prev, &next, &caps, &initial, 1u, out, sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_TRUE(rc == ZR_OK);

  ZR_ASSERT_TRUE(zr_golden_compare_fixture("diff_004_scroll_region_scroll_up_fullscreen", out, out_len) == 0);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

/*
 * Test: diff_002_style_change_single_glyph
 *
 * Scenario: Single character with style (red foreground, bold).
 *           Tests SGR sequence generation for RGB color + attributes.
 *
 * Arrange: 1x1 framebuffer, next='A' with fg=0xFF0000 (red), attrs=bold.
 * Act:     Render diff.
 * Assert:  Output matches pinned golden fixture with SGR codes.
 */
ZR_TEST_GOLDEN(diff_002_style_change_single_glyph) {
  /* --- Arrange --- */
  zr_fb_t prev;
  zr_fb_t next;
  (void)zr_fb_init(&prev, 1u, 1u);
  (void)zr_fb_init(&next, 1u, 1u);
  const zr_style_t s0 = zr_style_default();
  (void)zr_fb_clear(&prev, &s0);
  (void)zr_fb_clear(&next, &s0);

  zr_style_t s = s0;
  s.fg_rgb = 0xFF0000u; /* Red foreground */
  s.bg_rgb = 0x000000u;
  s.attrs = 1u; /* bold (v1) */
  zr_fb_set_ascii(&next, 0u, 0u, (uint8_t)'A', s);

  const plat_caps_t caps = zr_caps_rgb_all_attrs();
  const zr_term_state_t initial = zr_term_default();

  /* --- Act --- */
  uint8_t out[128];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc =
      zr_diff_render(&prev, &next, &caps, &initial, 0u, out, sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_TRUE(rc == ZR_OK);

  /* --- Assert --- */
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("diff_002_style_change_single_glyph", out, out_len) == 0);

  /* --- Cleanup --- */
  zr_fb_release(&prev);
  zr_fb_release(&next);
}

/*
 * Test: diff_003_wide_glyph_lead_only
 *
 * Scenario: Wide glyph (emoji U+1F642) at position (1,0). Tests that only
 *           the lead cell emits bytes; continuation cell is implicitly handled.
 *
 * Arrange: 4x1 framebuffer, emoji at x=1 (lead cell width=2, continuation at x=2).
 * Act:     Render diff.
 * Assert:  Output matches pinned golden fixture (CUP to x=1, then emoji bytes).
 */
ZR_TEST_GOLDEN(diff_003_wide_glyph_lead_only) {
  /* --- Arrange --- */
  zr_fb_t prev;
  zr_fb_t next;
  (void)zr_fb_init(&prev, 4u, 1u);
  (void)zr_fb_init(&next, 4u, 1u);
  const zr_style_t s = zr_style_default();
  (void)zr_fb_clear(&prev, &s);
  (void)zr_fb_clear(&next, &s);

  /* U+1F642 "slightly smiling face" in UTF-8 */
  const uint8_t emoji[4] = {0xF0u, 0x9Fu, 0x99u, 0x82u};
  zr_fb_set_utf8(&next, 1u, 0u, emoji, 4u, 2u, s); /* Lead cell */
  zr_fb_set_utf8(&next, 2u, 0u, (const uint8_t[4]){0u, 0u, 0u, 0u}, 0u, 0u, s); /* Continuation */

  const plat_caps_t caps = zr_caps_rgb_all_attrs();
  const zr_term_state_t initial = zr_term_default();

  /* --- Act --- */
  uint8_t out[128];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc =
      zr_diff_render(&prev, &next, &caps, &initial, 0u, out, sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_TRUE(rc == ZR_OK);

  /* --- Assert --- */
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("diff_003_wide_glyph_lead_only", out, out_len) == 0);

  /* --- Cleanup --- */
  zr_fb_release(&prev);
  zr_fb_release(&next);
}
