/*
  tests/unit/test_detect_overrides.c — Unit tests for capability override logic.

  Why: Locks force/suppress precedence so wrappers can deterministically control
  feature flags across environments.
*/

#include "zr_test.h"

#include "core/zr_detect.h"

#include <string.h>

static void zr_test_base_profile_caps(zr_terminal_profile_t* out_profile, plat_caps_t* out_caps) {
  if (!out_profile || !out_caps) {
    return;
  }
  memset(out_profile, 0, sizeof(*out_profile));
  memset(out_caps, 0, sizeof(*out_caps));

  out_profile->id = ZR_TERM_UNKNOWN;
  out_profile->supports_hyperlinks = 1u;
  out_profile->supports_grapheme_clusters = 0u;
  out_profile->supports_pixel_mouse = 0u;
  out_profile->supports_sync_update = 0u;
  out_profile->supports_mouse = 1u;

  out_caps->color_mode = PLAT_COLOR_MODE_RGB;
  out_caps->supports_mouse = 1u;
  out_caps->supports_bracketed_paste = 1u;
  out_caps->supports_focus_events = 0u;
  out_caps->supports_osc52 = 0u;
  out_caps->supports_sync_update = 0u;
  out_caps->supports_scroll_region = 1u;
  out_caps->supports_cursor_shape = 1u;
  out_caps->supports_output_wait_writable = 1u;
}

ZR_TEST_UNIT(detect_overrides_force_enables_capability) {
  zr_terminal_profile_t base_profile;
  plat_caps_t base_caps;
  zr_test_base_profile_caps(&base_profile, &base_caps);

  zr_terminal_profile_t out_profile;
  plat_caps_t out_caps;
  zr_detect_apply_overrides(&base_profile, &base_caps, ZR_TERM_CAP_PIXEL_MOUSE, 0u, &out_profile, &out_caps);

  ZR_ASSERT_EQ_U32(out_profile.supports_pixel_mouse, 1u);
}

ZR_TEST_UNIT(detect_overrides_suppress_disables_capability) {
  zr_terminal_profile_t base_profile;
  plat_caps_t base_caps;
  zr_test_base_profile_caps(&base_profile, &base_caps);

  zr_terminal_profile_t out_profile;
  plat_caps_t out_caps;
  zr_detect_apply_overrides(&base_profile, &base_caps, 0u, ZR_TERM_CAP_MOUSE, &out_profile, &out_caps);

  ZR_ASSERT_EQ_U32(out_caps.supports_mouse, 0u);
  ZR_ASSERT_EQ_U32(out_profile.supports_mouse, 0u);
  ZR_ASSERT_EQ_U32(out_profile.supports_pixel_mouse, 0u);
}

ZR_TEST_UNIT(detect_overrides_suppress_wins_over_force) {
  zr_terminal_profile_t base_profile;
  plat_caps_t base_caps;
  zr_test_base_profile_caps(&base_profile, &base_caps);

  const zr_terminal_cap_flags_t force = ZR_TERM_CAP_SYNC_UPDATE | ZR_TERM_CAP_MOUSE;
  const zr_terminal_cap_flags_t suppress = ZR_TERM_CAP_SYNC_UPDATE | ZR_TERM_CAP_MOUSE;

  zr_terminal_profile_t out_profile;
  plat_caps_t out_caps;
  zr_detect_apply_overrides(&base_profile, &base_caps, force, suppress, &out_profile, &out_caps);

  ZR_ASSERT_EQ_U32(out_caps.supports_sync_update, 0u);
  ZR_ASSERT_EQ_U32(out_profile.supports_sync_update, 0u);
  ZR_ASSERT_EQ_U32(out_caps.supports_mouse, 0u);
}

