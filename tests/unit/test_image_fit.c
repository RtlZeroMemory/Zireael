/*
  tests/unit/test_image_fit.c — Unit tests for deterministic RGBA fit modes.

  Why: Sixel/iTerm2 paths scale RGBA through this helper; pinning outputs keeps
  protocol bytes stable across refactors.
*/

#include "zr_test.h"

#include "core/zr_image.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void zr_px(uint8_t* dst, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  dst[0] = r;
  dst[1] = g;
  dst[2] = b;
  dst[3] = a;
}

ZR_TEST_UNIT(image_fit_fill_scales_with_nearest_neighbor) {
  uint8_t src[8];
  uint8_t out[16];
  uint8_t expected[16];

  zr_px(src + 0u, 255u, 0u, 0u, 255u);
  zr_px(src + 4u, 0u, 0u, 255u, 255u);

  zr_px(expected + 0u, 255u, 0u, 0u, 255u);
  zr_px(expected + 4u, 255u, 0u, 0u, 255u);
  zr_px(expected + 8u, 0u, 0u, 255u, 255u);
  zr_px(expected + 12u, 0u, 0u, 255u, 255u);

  ZR_ASSERT_EQ_U32(zr_image_scale_rgba(src, 2u, 1u, (uint8_t)ZR_IMAGE_FIT_FILL, 4u, 1u, out, sizeof(out)), ZR_OK);
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));
}

ZR_TEST_UNIT(image_fit_contain_letterboxes_transparent_pixels) {
  uint8_t src[8];
  uint8_t out[16];
  uint8_t expected[16];

  zr_px(src + 0u, 255u, 0u, 0u, 255u);
  zr_px(src + 4u, 0u, 0u, 255u, 255u);

  memset(expected, 0, sizeof(expected));
  zr_px(expected + 0u, 255u, 0u, 0u, 255u);
  zr_px(expected + 4u, 0u, 0u, 255u, 255u);

  ZR_ASSERT_EQ_U32(zr_image_scale_rgba(src, 2u, 1u, (uint8_t)ZR_IMAGE_FIT_CONTAIN, 2u, 2u, out, sizeof(out)), ZR_OK);
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));
}

ZR_TEST_UNIT(image_fit_cover_crops_center_region) {
  uint8_t src[8];
  uint8_t out[16];
  uint8_t expected[16];

  zr_px(src + 0u, 255u, 0u, 0u, 255u);
  zr_px(src + 4u, 0u, 0u, 255u, 255u);

  zr_px(expected + 0u, 255u, 0u, 0u, 255u);
  zr_px(expected + 4u, 0u, 0u, 255u, 255u);
  zr_px(expected + 8u, 255u, 0u, 0u, 255u);
  zr_px(expected + 12u, 0u, 0u, 255u, 255u);

  ZR_ASSERT_EQ_U32(zr_image_scale_rgba(src, 2u, 1u, (uint8_t)ZR_IMAGE_FIT_COVER, 2u, 2u, out, sizeof(out)), ZR_OK);
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected));
}

ZR_TEST_UNIT(image_fit_rejects_invalid_arguments) {
  uint8_t src[4] = {0u, 0u, 0u, 255u};
  uint8_t out[4] = {0u, 0u, 0u, 0u};

  ZR_ASSERT_EQ_U32(zr_image_scale_rgba(NULL, 1u, 1u, (uint8_t)ZR_IMAGE_FIT_FILL, 1u, 1u, out, sizeof(out)),
                   ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_scale_rgba(src, 1u, 1u, (uint8_t)ZR_IMAGE_FIT_FILL, 1u, 1u, NULL, sizeof(out)),
                   ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_scale_rgba(src, 0u, 1u, (uint8_t)ZR_IMAGE_FIT_FILL, 1u, 1u, out, sizeof(out)),
                   ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_scale_rgba(src, 1u, 1u, 9u, 1u, 1u, out, sizeof(out)), ZR_ERR_INVALID_ARGUMENT);

  ZR_ASSERT_EQ_U32(zr_image_scale_rgba(src, 1u, 1u, (uint8_t)ZR_IMAGE_FIT_FILL, 1u, 1u, out, 3u), ZR_ERR_LIMIT);
}
