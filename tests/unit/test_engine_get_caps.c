/*
  tests/unit/test_engine_get_caps.c — Unit tests for engine_get_caps (public ABI).

  Why: Ensures wrappers can query the engine’s runtime capability snapshot
  deterministically via the public API.
*/

#include "zr_test.h"

#include "core/zr_engine.h"

#include "unit/mock_platform.h"

#include <string.h>

static zr_engine_runtime_config_t zr_caps_runtime_from_create(const zr_engine_config_t* cfg) {
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

ZR_TEST_UNIT(engine_get_caps_reports_platform_caps) {
  mock_plat_reset();
  mock_plat_set_size(80u, 24u);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_256;
  caps.supports_mouse = 0u;
  caps.supports_bracketed_paste = 1u;
  caps.supports_focus_events = 0u;
  caps.supports_osc52 = 1u;
  caps.supports_sync_update = 1u;
  caps.supports_scroll_region = 0u;
  caps.supports_cursor_shape = 1u;
  caps.supports_output_wait_writable = 1u;
  caps.supports_underline_styles = 0u;
  caps.supports_colored_underlines = 0u;
  caps.supports_hyperlinks = 0u;
  caps.sgr_attrs_supported = 0x0Fu;
  mock_plat_set_caps(caps);

  zr_engine_config_t cfg = zr_engine_config_default();
  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_terminal_caps_t out;
  memset(&out, 0, sizeof(out));
  ZR_ASSERT_EQ_U32(engine_get_caps(e, &out), ZR_OK);

  ZR_ASSERT_EQ_U32((uint32_t)out.color_mode, (uint32_t)PLAT_COLOR_MODE_256);
  ZR_ASSERT_EQ_U32((uint32_t)out.supports_mouse, 0u);
  ZR_ASSERT_EQ_U32((uint32_t)out.supports_bracketed_paste, 1u);
  ZR_ASSERT_EQ_U32((uint32_t)out.supports_focus_events, 0u);
  ZR_ASSERT_EQ_U32((uint32_t)out.supports_osc52, 1u);
  ZR_ASSERT_EQ_U32((uint32_t)out.supports_sync_update, 1u);
  ZR_ASSERT_EQ_U32((uint32_t)out.supports_scroll_region, 0u);
  ZR_ASSERT_EQ_U32((uint32_t)out.supports_cursor_shape, 1u);
  ZR_ASSERT_EQ_U32((uint32_t)out.supports_output_wait_writable, 1u);
  ZR_ASSERT_EQ_U32(out.sgr_attrs_supported, 0x0Fu);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_get_terminal_profile_returns_stable_snapshot) {
  mock_plat_reset();
  mock_plat_set_size(80u, 24u);
  mock_plat_set_terminal_id_hint(ZR_TERM_WEZTERM);

  zr_engine_config_t cfg = zr_engine_config_default();
  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  const zr_terminal_profile_t* profile = engine_get_terminal_profile(e);
  ZR_ASSERT_TRUE(profile != NULL);
  ZR_ASSERT_EQ_U32((uint32_t)profile->id, (uint32_t)ZR_TERM_WEZTERM);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_set_config_updates_cap_overrides_in_caps_snapshot) {
  mock_plat_reset();
  mock_plat_set_size(80u, 24u);

  zr_engine_config_t cfg = zr_engine_config_default();
  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_engine_runtime_config_t runtime = zr_caps_runtime_from_create(&cfg);
  runtime.cap_force_flags = 0u;
  runtime.cap_suppress_flags = ZR_TERM_CAP_MOUSE;
  ZR_ASSERT_EQ_U32(engine_set_config(e, &runtime), ZR_OK);

  zr_terminal_caps_t caps_after_suppress;
  memset(&caps_after_suppress, 0, sizeof(caps_after_suppress));
  ZR_ASSERT_EQ_U32(engine_get_caps(e, &caps_after_suppress), ZR_OK);
  ZR_ASSERT_EQ_U32(caps_after_suppress.supports_mouse, 0u);
  ZR_ASSERT_EQ_U32(caps_after_suppress.cap_suppress_flags, ZR_TERM_CAP_MOUSE);

  runtime.cap_force_flags = ZR_TERM_CAP_MOUSE;
  runtime.cap_suppress_flags = 0u;
  ZR_ASSERT_EQ_U32(engine_set_config(e, &runtime), ZR_OK);

  zr_terminal_caps_t caps_after_force;
  memset(&caps_after_force, 0, sizeof(caps_after_force));
  ZR_ASSERT_EQ_U32(engine_get_caps(e, &caps_after_force), ZR_OK);
  ZR_ASSERT_EQ_U32(caps_after_force.supports_mouse, 1u);
  ZR_ASSERT_EQ_U32(caps_after_force.cap_force_flags, ZR_TERM_CAP_MOUSE);

  engine_destroy(e);
}
