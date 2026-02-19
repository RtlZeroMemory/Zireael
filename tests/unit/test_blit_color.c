/*
  tests/unit/test_blit_color.c — Unit tests for blitter color/luma/alpha helpers.

  Why: Pins deterministic math primitives used by all sub-cell blitters.
*/

#include "zr_test.h"

#include "core/zr_blit.h"

ZR_TEST_UNIT(blit_color_distance_known_pairs) {
  const uint32_t black = zr_blit_pack_rgb(0u, 0u, 0u);
  const uint32_t white = zr_blit_pack_rgb(255u, 255u, 255u);
  const uint32_t red = zr_blit_pack_rgb(255u, 0u, 0u);
  const uint32_t green = zr_blit_pack_rgb(0u, 255u, 0u);

  ZR_ASSERT_EQ_U32(zr_blit_rgb_distance_sq(black, white), 195075u);
  ZR_ASSERT_EQ_U32(zr_blit_rgb_distance_sq(red, green), 130050u);
  ZR_ASSERT_EQ_U32(zr_blit_rgb_distance_sq(black, black), 0u);
}

ZR_TEST_UNIT(blit_luma_bt709_known_values) {
  const uint32_t black = zr_blit_pack_rgb(0u, 0u, 0u);
  const uint32_t white = zr_blit_pack_rgb(255u, 255u, 255u);
  const uint32_t red = zr_blit_pack_rgb(255u, 0u, 0u);

  ZR_ASSERT_EQ_U32(zr_blit_luma_bt709(black), 0u);
  ZR_ASSERT_EQ_U32(zr_blit_luma_bt709(white), 255u);
  ZR_ASSERT_EQ_U32(zr_blit_luma_bt709(red), 54u);
}

ZR_TEST_UNIT(blit_alpha_threshold_binary) {
  ZR_ASSERT_EQ_U32(zr_blit_alpha_is_opaque(0u), 0u);
  ZR_ASSERT_EQ_U32(zr_blit_alpha_is_opaque(127u), 0u);
  ZR_ASSERT_EQ_U32(zr_blit_alpha_is_opaque(128u), 1u);
  ZR_ASSERT_EQ_U32(zr_blit_alpha_is_opaque(255u), 1u);
}
