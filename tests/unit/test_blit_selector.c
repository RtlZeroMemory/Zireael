/*
  tests/unit/test_blit_selector.c — Unit tests for blitter AUTO/exact selection.

  Why: Pins deterministic mode selection across terminal capability combinations.
*/

#include "zr_test.h"

#include "core/zr_blit.h"

static zr_blit_caps_t zr_caps_base(void) {
  zr_blit_caps_t c;
  c.terminal_id = ZR_TERM_UNKNOWN;
  c.is_dumb_terminal = 0u;
  c.is_pipe_mode = 0u;
  c.supports_unicode = 1u;
  c.supports_quadrant = 1u;
  c.supports_sextant = 0u;
  c.supports_halfblock = 1u;
  c.supports_braille = 1u;
  c.include_braille_in_auto = 0u;
  c._pad0[0] = 0u;
  c._pad0[1] = 0u;
  c._pad0[2] = 0u;
  return c;
}

ZR_TEST_UNIT(blit_selector_auto_prefers_sextant_when_supported) {
  zr_blit_caps_t c = zr_caps_base();
  zr_blitter_t out = ZR_BLIT_ASCII;
  c.supports_sextant = 1u;
  c.terminal_id = ZR_TERM_KITTY;
  ZR_ASSERT_EQ_U32(zr_blit_select(ZR_BLIT_AUTO, &c, &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out, ZR_BLIT_SEXTANT);
}

ZR_TEST_UNIT(blit_selector_auto_chooses_quadrant_without_sextant) {
  zr_blit_caps_t c = zr_caps_base();
  zr_blitter_t out = ZR_BLIT_ASCII;
  c.supports_sextant = 0u;
  ZR_ASSERT_EQ_U32(zr_blit_select(ZR_BLIT_AUTO, &c, &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out, ZR_BLIT_QUADRANT);
}

ZR_TEST_UNIT(blit_selector_auto_chooses_ascii_for_dumb_terminal) {
  zr_blit_caps_t c = zr_caps_base();
  zr_blitter_t out = ZR_BLIT_QUADRANT;
  c.is_dumb_terminal = 1u;
  ZR_ASSERT_EQ_U32(zr_blit_select(ZR_BLIT_AUTO, &c, &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out, ZR_BLIT_ASCII);
}

ZR_TEST_UNIT(blit_selector_explicit_braille_is_honored) {
  zr_blit_caps_t c = zr_caps_base();
  zr_blitter_t out = ZR_BLIT_ASCII;
  c.supports_braille = 0u;
  ZR_ASSERT_EQ_U32(zr_blit_select(ZR_BLIT_BRAILLE, &c, &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out, ZR_BLIT_BRAILLE);
}

ZR_TEST_UNIT(blit_selector_explicit_sextant_no_downgrade) {
  zr_blit_caps_t c = zr_caps_base();
  zr_blitter_t out = ZR_BLIT_ASCII;
  c.supports_sextant = 0u;
  ZR_ASSERT_EQ_U32(zr_blit_select(ZR_BLIT_SEXTANT, &c, &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out, ZR_BLIT_SEXTANT);
}

ZR_TEST_UNIT(blit_selector_auto_can_include_braille) {
  zr_blit_caps_t c = zr_caps_base();
  zr_blitter_t out = ZR_BLIT_ASCII;
  c.include_braille_in_auto = 1u;
  ZR_ASSERT_EQ_U32(zr_blit_select(ZR_BLIT_AUTO, &c, &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out, ZR_BLIT_BRAILLE);
}

ZR_TEST_UNIT(blit_selector_pixel_mode_is_unsupported) {
  zr_blit_caps_t c = zr_caps_base();
  zr_blitter_t out = ZR_BLIT_ASCII;
  ZR_ASSERT_EQ_U32(zr_blit_select(ZR_BLIT_PIXEL, &c, &out), ZR_ERR_UNSUPPORTED);
}
