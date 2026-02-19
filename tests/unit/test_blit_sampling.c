/*
  tests/unit/test_blit_sampling.c — Unit tests for nearest-neighbor sub-pixel sampling.

  Why: Locks the integer mapping formula used to scale pixel buffers into cell grids.
*/

#include "zr_test.h"

#include "core/zr_blit.h"

#include <string.h>

ZR_TEST_UNIT(blit_sample_axis_exact_and_scaled_cases) {
  ZR_ASSERT_EQ_U32(zr_blit_sample_axis(0u, 8u, 4u, 2u), 0u);
  ZR_ASSERT_EQ_U32(zr_blit_sample_axis(7u, 8u, 4u, 2u), 7u);

  ZR_ASSERT_EQ_U32(zr_blit_sample_axis(0u, 8u, 2u, 1u), 0u);
  ZR_ASSERT_EQ_U32(zr_blit_sample_axis(1u, 8u, 2u, 1u), 4u);

  ZR_ASSERT_EQ_U32(zr_blit_sample_axis(0u, 2u, 4u, 1u), 0u);
  ZR_ASSERT_EQ_U32(zr_blit_sample_axis(1u, 2u, 4u, 1u), 0u);
  ZR_ASSERT_EQ_U32(zr_blit_sample_axis(2u, 2u, 4u, 1u), 1u);
  ZR_ASSERT_EQ_U32(zr_blit_sample_axis(3u, 2u, 4u, 1u), 1u);

  ZR_ASSERT_EQ_U32(zr_blit_sample_axis(2u, 5u, 3u, 1u), 3u);
}

ZR_TEST_UNIT(blit_sample_subpixel_reads_expected_rgba) {
  uint8_t pixels[32];
  memset(pixels, 0, sizeof(pixels));

  /* 4x2 RGBA image, stride=16. Pixel (2,1) = {10,20,30,255}. */
  pixels[16u + 8u + 0u] = 10u;
  pixels[16u + 8u + 1u] = 20u;
  pixels[16u + 8u + 2u] = 30u;
  pixels[16u + 8u + 3u] = 255u;

  zr_blit_input_t in;
  in.pixels = pixels;
  in.px_width = 4u;
  in.px_height = 2u;
  in.stride = 16u;

  uint8_t out[4] = {0u, 0u, 0u, 0u};
  ZR_ASSERT_EQ_U32(zr_blit_sample_subpixel(&in, 2u, 1u, 4u, 2u, 1u, 1u, out), ZR_OK);
  ZR_ASSERT_EQ_U32(out[0], 10u);
  ZR_ASSERT_EQ_U32(out[1], 20u);
  ZR_ASSERT_EQ_U32(out[2], 30u);
  ZR_ASSERT_EQ_U32(out[3], 255u);
}
