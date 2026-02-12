/*
  tests/unit/test_diff_color_quant.c — Unit tests for diff color quantization.

  Why: Prevents regressions in deterministic RGB→xterm256 mapping used by the
  diff renderer when truecolor is unavailable.
*/

#include "zr_test.h"

#include "core/zr_diff.h"
#include "core/zr_framebuffer.h"
#include "platform/zr_platform.h"

#include <string.h>

ZR_TEST_UNIT(diff_xterm256_component_distance_is_symmetric) {
  zr_fb_t prev;
  zr_fb_t next;
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 1u, 1u), ZR_OK);

  zr_style_t base;
  base.fg_rgb = 0u;
  base.bg_rgb = 0u;
  base.attrs = 0u;
  base.reserved = 0u;
  (void)zr_fb_clear(&prev, &base);
  (void)zr_fb_clear(&next, &base);

  /* RGB=(125,0,0) should quantize to the 6x6x6 cube r=135 component (index 2). */
  zr_style_t s = base;
  s.fg_rgb = 0x7D0000u;

  zr_cell_t* c = zr_fb_cell(&next, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = (uint8_t)'X';
  c->glyph_len = 1u;
  c->width = 1u;
  c->style = s;

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_256;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.flags = ZR_TERM_STATE_VALID_ALL;
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

  /* Expected: ESC[38;5;88;48;5;16mX (no CUP, cursor starts at 0,0). */
  const uint8_t expected[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'3', (uint8_t)'8', (uint8_t)';', (uint8_t)'5', (uint8_t)';',
      (uint8_t)'8', (uint8_t)'8', (uint8_t)';', (uint8_t)'4', (uint8_t)'8', (uint8_t)';', (uint8_t)'5',
      (uint8_t)';', (uint8_t)'1', (uint8_t)'6', (uint8_t)'m', (uint8_t)'X',
  };
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}

ZR_TEST_UNIT(diff_ansi16_emits_standard_fg_bg_codes) {
  zr_fb_t prev;
  zr_fb_t next;
  ZR_ASSERT_EQ_U32(zr_fb_init(&prev, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&next, 1u, 1u), ZR_OK);

  zr_style_t base;
  base.fg_rgb = 0u;
  base.bg_rgb = 0u;
  base.attrs = 0u;
  base.reserved = 0u;
  (void)zr_fb_clear(&prev, &base);
  (void)zr_fb_clear(&next, &base);

  zr_style_t s = base;
  s.fg_rgb = 0x00CD0000u; /* ANSI 16 index 1 => SGR 31. */
  s.bg_rgb = 0x000000EEu; /* ANSI 16 index 4 => SGR 44. */

  zr_cell_t* c = zr_fb_cell(&next, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = (uint8_t)'X';
  c->glyph_len = 1u;
  c->width = 1u;
  c->style = s;

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_16;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.flags = ZR_TERM_STATE_VALID_ALL;
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

  /* Expected: ESC[31;44mX (no CUP, cursor starts at 0,0). */
  const uint8_t expected[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'3', (uint8_t)'1', (uint8_t)';',
      (uint8_t)'4', (uint8_t)'4', (uint8_t)'m', (uint8_t)'X',
  };
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));

  zr_fb_release(&prev);
  zr_fb_release(&next);
}
