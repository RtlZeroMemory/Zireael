/*
  tests/unit/test_diff_color_quant.c — Unit tests for diff color quantization.

  Why: Prevents regressions in deterministic RGB→xterm256 mapping used by the
  diff renderer when truecolor is unavailable.
*/

#include "zr_test.h"

#include "core/zr_diff.h"
#include "core/zr_fb.h"
#include "platform/zr_platform.h"

#include <string.h>

ZR_TEST_UNIT(diff_xterm256_component_distance_is_symmetric) {
  zr_fb_cell_t prev_cells[1];
  zr_fb_cell_t next_cells[1];
  zr_fb_t prev;
  zr_fb_t next;
  (void)zr_fb_init(&prev, prev_cells, 1u, 1u);
  (void)zr_fb_init(&next, next_cells, 1u, 1u);

  zr_style_t base;
  base.fg = 0u;
  base.bg = 0u;
  base.attrs = 0u;
  (void)zr_fb_clear(&prev, &base);
  (void)zr_fb_clear(&next, &base);

  /* RGB=(125,0,0) should quantize to the 6x6x6 cube r=135 component (index 2). */
  zr_style_t s = base;
  s.fg = 0x7D0000u;

  zr_fb_cell_t* c = zr_fb_cell_at(&next, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = (uint8_t)'X';
  c->glyph_len = 1u;
  c->flags = 0u;
  c->style = s;

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_256;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.cursor_x = 0u;
  initial.cursor_y = 0u;
  initial.style = base;

  uint8_t out[128];
  size_t out_len = 0u;
  zr_term_state_t final_state;
  zr_diff_stats_t stats;
  const zr_result_t rc =
      zr_diff_render(&prev, &next, &caps, &initial, out, sizeof(out), &out_len, &final_state, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Expected: ESC[0;38;5;88;48;5;16mX (no CUP, cursor starts at 0,0). */
  const uint8_t expected[] = {
      0x1Bu, (uint8_t)'[', (uint8_t)'0', (uint8_t)';', (uint8_t)'3', (uint8_t)'8', (uint8_t)';',
      (uint8_t)'5', (uint8_t)';', (uint8_t)'8', (uint8_t)'8', (uint8_t)';', (uint8_t)'4',
      (uint8_t)'8', (uint8_t)';', (uint8_t)'5', (uint8_t)';', (uint8_t)'1', (uint8_t)'6',
      (uint8_t)'m', (uint8_t)'X',
  };
  ZR_ASSERT_EQ_U32(out_len, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));
}
