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

static zr_engine_runtime_config_t zr_runtime_cfg_from_create_cfg(const zr_engine_config_t* cfg) {
  zr_engine_runtime_config_t runtime = {0};
  if (!cfg) {
    return runtime;
  }
  runtime.limits = cfg->limits;
  runtime.plat = cfg->plat;
  runtime.tab_width = cfg->tab_width;
  runtime.width_policy = cfg->width_policy;
  runtime.target_fps = cfg->target_fps;
  runtime.enable_scroll_optimizations = cfg->enable_scroll_optimizations;
  runtime.enable_debug_overlay = cfg->enable_debug_overlay;
  runtime.enable_replay_recording = cfg->enable_replay_recording;
  runtime.wait_for_output_drain = cfg->wait_for_output_drain;
  runtime.cap_force_flags = cfg->cap_force_flags;
  runtime.cap_suppress_flags = cfg->cap_suppress_flags;
  return runtime;
}

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

ZR_TEST_UNIT(engine_create_wait_for_output_drain_unsupported_fails_early) {
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

  /*
    engine_create must reject drain-on-unsupported-backend at creation time
    rather than letting every engine_present() call fail.
  */
  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_ERR_UNSUPPORTED);
  ZR_ASSERT_TRUE(e == NULL);
}

ZR_TEST_UNIT(engine_create_wait_for_output_drain_disabled_ok_without_cap) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  plat_caps_t caps = {0};
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_scroll_region = 1u;
  caps.supports_output_wait_writable = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;
  mock_plat_set_caps(caps);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;
  cfg.wait_for_output_drain = 0u;

  /* Drain disabled: create must succeed even without backend support. */
  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_set_config_wait_for_output_drain_unsupported_rejected_without_mutation) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_output_writable(0u);

  plat_caps_t caps = {0};
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_scroll_region = 1u;
  caps.supports_output_wait_writable = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;
  mock_plat_set_caps(caps);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;
  cfg.wait_for_output_drain = 0u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_engine_runtime_config_t runtime = zr_runtime_cfg_from_create_cfg(&cfg);
  runtime.wait_for_output_drain = 1u;

  /*
    Rejected runtime config must leave the active config unchanged. If drain
    were accidentally enabled, present would call wait_output_writable and fail.
  */
  ZR_ASSERT_EQ_U32(engine_set_config(e, &runtime), ZR_ERR_UNSUPPORTED);

  ZR_ASSERT_EQ_U32(engine_submit_drawlist(e, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len), ZR_OK);

  mock_plat_clear_writes();
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);
  ZR_ASSERT_EQ_U32(mock_plat_wait_output_call_count(), 0u);

  engine_destroy(e);
}
