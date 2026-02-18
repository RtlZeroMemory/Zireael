/*
  tests/unit/test_diff_term_state_validity.c — Diff renderer terminal-state validity edge cases.

  Why: The diff renderer's output correctness depends on its assumed initial
  terminal state. When the engine knows the terminal cursor/style state may be
  desynced (startup, resize), it must be able to force re-establishment of
  cursor position / SGR / cursor shape on the next render without changing the
  public ABI.
*/

#include "zr_test.h"

#include "core/zr_diff.h"
#include "core/zr_framebuffer.h"
#include "platform/zr_platform.h"

#include <string.h>

static zr_style_t zr_style_black_on_black(void) {
  zr_style_t s;
  s.fg_rgb = 0u;
  s.bg_rgb = 0u;
  s.attrs = 0u;
  s.reserved = 0u;
  return s;
}

static void zr_cell_set_ascii(zr_test_ctx_t* ctx, zr_cell_t* c, uint8_t ch, zr_style_t style) {
  ZR_ASSERT_TRUE(c != NULL);
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = ch;
  c->glyph_len = 1u;
  c->width = 1u;
  c->style = style;
}

ZR_TEST_UNIT(diff_unknown_cursor_pos_forces_cup_even_at_home) {
  zr_fb_t prev;
  zr_fb_t next;
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 1u, 1u), ZR_OK);

  const zr_style_t base = zr_style_black_on_black();
  (void)zr_fb_clear(&prev, &base);
  (void)zr_fb_clear(&next, &base);

  zr_cell_set_ascii(ctx, zr_fb_cell(&next, 0u, 0u), (uint8_t)'X', base);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_cursor_shape = 1u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.cursor_visible = 0u;
  initial.cursor_shape = ZR_CURSOR_SHAPE_BLOCK;
  initial.cursor_blink = 0u;
  initial.flags =
      (uint8_t)(ZR_TERM_STATE_STYLE_VALID | ZR_TERM_STATE_CURSOR_VIS_VALID | ZR_TERM_STATE_CURSOR_SHAPE_VALID |
                ZR_TERM_STATE_SCREEN_VALID);
  initial.style = base;

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

  /* Expected: ESC[1;1HX (CUP forced due to unknown cursor-pos validity). */
  const uint8_t expected[] = {
      0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'1', (uint8_t)'H', (uint8_t)'X',
  };
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_unknown_cursor_pos_forces_cup_without_frame_damage) {
  zr_fb_t prev;
  zr_fb_t next;
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 1u, 1u), ZR_OK);

  const zr_style_t base = zr_style_black_on_black();
  (void)zr_fb_clear(&prev, &base);
  (void)zr_fb_clear(&next, &base);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_cursor_shape = 1u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.cursor_visible = 0u;
  initial.cursor_shape = ZR_CURSOR_SHAPE_BLOCK;
  initial.cursor_blink = 0u;
  initial.flags =
      (uint8_t)(ZR_TERM_STATE_STYLE_VALID | ZR_TERM_STATE_CURSOR_VIS_VALID | ZR_TERM_STATE_CURSOR_SHAPE_VALID |
                ZR_TERM_STATE_SCREEN_VALID);
  initial.style = base;

  zr_cursor_state_t desired;
  desired.x = -1;
  desired.y = -1;
  desired.shape = ZR_CURSOR_SHAPE_BLOCK;
  desired.visible = 0u;
  desired.blink = 0u;
  desired.reserved0 = 0u;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[128];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render(&prev, &next, &caps, &initial, &desired, &lim, damage, 64u, 0u, out,
                                        sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Expected: ESC[1;1H (CUP forced because cursor position validity was unknown). */
  const uint8_t expected[] = {
      0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'1', (uint8_t)'H',
  };
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_unknown_style_forces_absolute_sgr_even_if_values_match) {
  zr_fb_t prev;
  zr_fb_t next;
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 1u, 1u), ZR_OK);

  const zr_style_t base = zr_style_black_on_black();
  (void)zr_fb_clear(&prev, &base);
  (void)zr_fb_clear(&next, &base);

  zr_cell_set_ascii(ctx, zr_fb_cell(&next, 0u, 0u), (uint8_t)'X', base);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_cursor_shape = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.cursor_visible = 0u;
  initial.cursor_shape = ZR_CURSOR_SHAPE_BLOCK;
  initial.cursor_blink = 0u;
  initial.flags =
      (uint8_t)(ZR_TERM_STATE_CURSOR_POS_VALID | ZR_TERM_STATE_CURSOR_VIS_VALID | ZR_TERM_STATE_CURSOR_SHAPE_VALID |
                ZR_TERM_STATE_SCREEN_VALID);
  initial.style = base;

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

  /* Expected: ESC[0;38;2;0;0;0;48;2;0;0;0mX (style forced absolute due to unknown style validity). */
  const uint8_t expected[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'0', (uint8_t)';', (uint8_t)'3', (uint8_t)'8', (uint8_t)';',
      (uint8_t)'2', (uint8_t)';', (uint8_t)'0', (uint8_t)';', (uint8_t)'0', (uint8_t)';', (uint8_t)'0',
      (uint8_t)';', (uint8_t)'4', (uint8_t)'8', (uint8_t)';', (uint8_t)'2', (uint8_t)';', (uint8_t)'0',
      (uint8_t)';', (uint8_t)'0', (uint8_t)';', (uint8_t)'0', (uint8_t)'m', (uint8_t)'X',
  };
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_unknown_cursor_shape_emits_decsusr_when_showing_cursor) {
  zr_fb_t prev;
  zr_fb_t next;
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 1u, 1u), ZR_OK);

  const zr_style_t base = zr_style_black_on_black();
  (void)zr_fb_clear(&prev, &base);
  (void)zr_fb_clear(&next, &base);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_cursor_shape = 1u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.cursor_visible = 0u;
  initial.cursor_shape = ZR_CURSOR_SHAPE_BLOCK;
  initial.cursor_blink = 0u;
  initial.flags =
      (uint8_t)(ZR_TERM_STATE_CURSOR_POS_VALID | ZR_TERM_STATE_STYLE_VALID | ZR_TERM_STATE_CURSOR_VIS_VALID |
                ZR_TERM_STATE_SCREEN_VALID);
  initial.style = base;

  zr_cursor_state_t desired;
  desired.x = -1;
  desired.y = -1;
  desired.shape = ZR_CURSOR_SHAPE_BLOCK;
  desired.visible = 1u;
  desired.blink = 0u;
  desired.reserved0 = 0u;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[128];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render(&prev, &next, &caps, &initial, &desired, &lim, damage, 64u, 0u, out,
                                        sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Expected: ESC[2 q ESC[?25h (DECSCUSR steady block + show cursor). */
  const uint8_t expected[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)' ', (uint8_t)'q', 0x1Bu,
      (uint8_t)'[', (uint8_t)'?', (uint8_t)'2', (uint8_t)'5', (uint8_t)'h',
  };
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_hides_cursor_with_vt_sequence_when_requested) {
  zr_fb_t prev;
  zr_fb_t next;
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 1u, 1u), ZR_OK);

  const zr_style_t base = zr_style_black_on_black();
  (void)zr_fb_clear(&prev, &base);
  (void)zr_fb_clear(&next, &base);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_cursor_shape = 1u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.cursor_visible = 1u;
  initial.cursor_shape = ZR_CURSOR_SHAPE_BLOCK;
  initial.cursor_blink = 0u;
  initial.flags = ZR_TERM_STATE_VALID_ALL;
  initial.style = base;

  zr_cursor_state_t desired;
  desired.x = -1;
  desired.y = -1;
  desired.shape = ZR_CURSOR_SHAPE_BLOCK;
  desired.visible = 0u;
  desired.blink = 0u;
  desired.reserved0 = 0u;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[128];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc = zr_diff_render(&prev, &next, &caps, &initial, &desired, &lim, damage, 64u, 0u, out,
                                        sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Expected: ESC[?25l (hide cursor only; no CUP needed because cursor pos is valid). */
  const uint8_t expected[] = {
      0x1Bu, (uint8_t)'[', (uint8_t)'?', (uint8_t)'2', (uint8_t)'5', (uint8_t)'l',
  };
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_screen_invalid_establishes_blank_baseline) {
  zr_fb_t prev;
  zr_fb_t next;
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 1u, 1u), ZR_OK);

  const zr_style_t base = zr_style_black_on_black();
  (void)zr_fb_clear(&prev, &base);
  (void)zr_fb_clear(&next, &base);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_cursor_shape = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.cursor_visible = 0u;
  initial.cursor_shape = ZR_CURSOR_SHAPE_BLOCK;
  initial.cursor_blink = 0u;
  initial.flags = (uint8_t)(ZR_TERM_STATE_STYLE_VALID | ZR_TERM_STATE_CURSOR_POS_VALID);
  initial.style = base;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[256];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc =
      zr_diff_render(&prev, &next, &caps, &initial, NULL, &lim, damage, 64u, 0u, out, sizeof(out), &out_len,
                     &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  static const uint8_t expected[] =
      "\x1b[r"
      "\x1b[0;38;2;0;0;0;48;2;0;0;0m"
      "\x1b[2J";
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)(sizeof(expected) - 1u));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected) - 1u);
  ZR_ASSERT_TRUE((final_state.flags & ZR_TERM_STATE_SCREEN_VALID) != 0u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_screen_valid_does_not_emit_baseline_clear) {
  zr_fb_t prev;
  zr_fb_t next;
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 1u, 1u), ZR_OK);

  const zr_style_t base = zr_style_black_on_black();
  (void)zr_fb_clear(&prev, &base);
  (void)zr_fb_clear(&next, &base);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_cursor_shape = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.cursor_visible = 0u;
  initial.cursor_shape = ZR_CURSOR_SHAPE_BLOCK;
  initial.cursor_blink = 0u;
  initial.flags = (uint8_t)(ZR_TERM_STATE_STYLE_VALID | ZR_TERM_STATE_CURSOR_POS_VALID | ZR_TERM_STATE_SCREEN_VALID);
  initial.style = base;

  zr_limits_t lim = zr_limits_default();
  lim.diff_max_damage_rects = 64u;
  zr_damage_rect_t damage[64];

  uint8_t out[256];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc =
      zr_diff_render(&prev, &next, &caps, &initial, NULL, &lim, damage, 64u, 0u, out, sizeof(out), &out_len,
                     &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(out_len, 0u);

  zr_fb_release(&prev);
  zr_fb_release(&next);
}
