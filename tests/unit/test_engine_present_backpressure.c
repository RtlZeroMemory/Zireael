/*
  tests/unit/test_engine_present_backpressure.c — Unit tests for output backpressure pacing.

  Why: Verifies the optional wait-for-output-drain policy blocks frame emission
  when output is not writable and preserves the single-flush/no-partial-effects
  contracts.
*/

#include "zr_test.h"

#include "core/zr_engine.h"

#include "unit/mock_platform.h"

extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;

ZR_TEST_UNIT(engine_present_wait_for_output_drain_times_out_without_writes) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_output_writable(0u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;
  cfg.wait_for_output_drain = 1u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  mock_plat_clear_writes();
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 0u);
  ZR_ASSERT_EQ_U32(mock_plat_wait_output_call_count(), 1u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_wait_for_output_drain_succeeds_when_writable) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_output_writable(1u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;
  cfg.wait_for_output_drain = 1u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_EQ_U32(engine_submit_drawlist(e, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len), ZR_OK);

  mock_plat_clear_writes();
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);
  ZR_ASSERT_EQ_U32(mock_plat_wait_output_call_count(), 1u);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_wait_for_output_drain_unsupported_propagates) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_output_writable(1u);

  plat_caps_t caps = {0};
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_scroll_region = 1u;
  caps.supports_output_wait_writable = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;
  mock_plat_set_caps(caps);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;
  cfg.wait_for_output_drain = 1u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  mock_plat_clear_writes();
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_ERR_UNSUPPORTED);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 0u);
  ZR_ASSERT_EQ_U32(mock_plat_wait_output_call_count(), 1u);

  engine_destroy(e);
}

