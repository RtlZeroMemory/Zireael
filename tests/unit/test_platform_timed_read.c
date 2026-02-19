/*
  tests/unit/test_platform_timed_read.c — Unit tests for timed platform reads.

  Why: Verifies the platform timed-read primitive contract used by startup
  detection remains deterministic in the unit-test mock backend.
*/

#include "zr_test.h"

#include "unit/mock_platform.h"

#include <string.h>

ZR_TEST_UNIT(platform_timed_read_returns_available_bytes) {
  mock_plat_reset();

  const plat_config_t cfg = {
      .requested_color_mode = PLAT_COLOR_MODE_UNKNOWN,
      .enable_mouse = 1u,
      .enable_bracketed_paste = 1u,
      .enable_focus_events = 1u,
      .enable_osc52 = 1u,
      ._pad = {0u, 0u, 0u},
  };
  plat_t* plat = NULL;
  ZR_ASSERT_EQ_U32(plat_create(&plat, &cfg), ZR_OK);

  static const uint8_t kInput[] = "abc";
  ZR_ASSERT_EQ_U32(mock_plat_push_input(kInput, sizeof(kInput) - 1u), ZR_OK);

  uint8_t out[8];
  memset(out, 0, sizeof(out));
  const int32_t n = plat_read_input_timed(plat, out, (int32_t)sizeof(out), 100);
  ZR_ASSERT_EQ_U32((uint32_t)n, 3u);
  ZR_ASSERT_TRUE(out[0] == (uint8_t)'a' && out[1] == (uint8_t)'b' && out[2] == (uint8_t)'c');

  plat_destroy(plat);
}

ZR_TEST_UNIT(platform_timed_read_timeout_returns_zero) {
  mock_plat_reset();

  const plat_config_t cfg = {
      .requested_color_mode = PLAT_COLOR_MODE_UNKNOWN,
      .enable_mouse = 1u,
      .enable_bracketed_paste = 1u,
      .enable_focus_events = 1u,
      .enable_osc52 = 1u,
      ._pad = {0u, 0u, 0u},
  };
  plat_t* plat = NULL;
  ZR_ASSERT_EQ_U32(plat_create(&plat, &cfg), ZR_OK);

  uint8_t out[4];
  const int32_t n = plat_read_input_timed(plat, out, (int32_t)sizeof(out), 100);
  ZR_ASSERT_EQ_U32((uint32_t)n, 0u);

  plat_destroy(plat);
}

ZR_TEST_UNIT(platform_timed_read_invalid_timeout_rejected) {
  mock_plat_reset();

  const plat_config_t cfg = {
      .requested_color_mode = PLAT_COLOR_MODE_UNKNOWN,
      .enable_mouse = 1u,
      .enable_bracketed_paste = 1u,
      .enable_focus_events = 1u,
      .enable_osc52 = 1u,
      ._pad = {0u, 0u, 0u},
  };
  plat_t* plat = NULL;
  ZR_ASSERT_EQ_U32(plat_create(&plat, &cfg), ZR_OK);

  uint8_t out[4];
  const int32_t n = plat_read_input_timed(plat, out, (int32_t)sizeof(out), -1);
  ZR_ASSERT_EQ_U32((uint32_t)n, (uint32_t)ZR_ERR_INVALID_ARGUMENT);

  plat_destroy(plat);
}
