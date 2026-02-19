/*
  tests/unit/test_detect_profile.c — Unit coverage for profile construction.

  Why: Ensures startup detection builds stable profiles and fallback identity
  behavior without requiring a real terminal.
*/

#include "zr_test.h"

#include "core/zr_detect.h"

#include "unit/mock_platform.h"

static zr_result_t zr_test_open_mock_platform(plat_t** out_plat, plat_caps_t* out_caps) {
  if (!out_plat || !out_caps) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_plat = NULL;

  const plat_config_t cfg = {
      .requested_color_mode = PLAT_COLOR_MODE_UNKNOWN,
      .enable_mouse = 1u,
      .enable_bracketed_paste = 1u,
      .enable_focus_events = 1u,
      .enable_osc52 = 1u,
      ._pad = {0u, 0u, 0u},
  };

  zr_result_t rc = plat_create(out_plat, &cfg);
  if (rc != ZR_OK) {
    return rc;
  }

  rc = plat_enter_raw(*out_plat);
  if (rc != ZR_OK) {
    plat_destroy(*out_plat);
    *out_plat = NULL;
    return rc;
  }

  rc = plat_get_caps(*out_plat, out_caps);
  if (rc != ZR_OK) {
    (void)plat_leave_raw(*out_plat);
    plat_destroy(*out_plat);
    *out_plat = NULL;
    return rc;
  }
  return ZR_OK;
}

static void zr_test_close_mock_platform(plat_t* plat) {
  if (!plat) {
    return;
  }
  (void)plat_leave_raw(plat);
  plat_destroy(plat);
}

ZR_TEST_UNIT(detect_profile_known_terminal_kitty) {
  mock_plat_reset();

  plat_t* plat = NULL;
  plat_caps_t baseline;
  ZR_ASSERT_EQ_U32(zr_test_open_mock_platform(&plat, &baseline), ZR_OK);

  static const uint8_t kResponses[] =
      "\x1bP>|kitty(0.35.0)\x1b\\"
      "\x1b[?1;2;22c"
      "\x1b[>1;3500;0c"
      "\x1b[?2026;1$y"
      "\x1b[?2027;1$y"
      "\x1b[?1016;1$y"
      "\x1b[?2004;1$y"
      "\x1b[6;20;10t";
  ZR_ASSERT_EQ_U32(mock_plat_push_input(kResponses, sizeof(kResponses) - 1u), ZR_OK);

  zr_terminal_profile_t profile;
  plat_caps_t out_caps;
  ZR_ASSERT_EQ_U32(zr_detect_probe_terminal(plat, &baseline, &profile, &out_caps), ZR_OK);

  ZR_ASSERT_EQ_U32((uint32_t)profile.id, (uint32_t)ZR_TERM_KITTY);
  ZR_ASSERT_EQ_U32(profile.xtversion_responded, 1u);
  ZR_ASSERT_EQ_U32(profile.supports_kitty_graphics, 1u);
  ZR_ASSERT_EQ_U32(profile.supports_sixel, 0u);
  ZR_ASSERT_EQ_U32(profile.supports_pixel_mouse, 1u);
  ZR_ASSERT_EQ_U32(profile.supports_grapheme_clusters, 1u);
  ZR_ASSERT_EQ_U32(profile.supports_bracketed_paste, 1u);
  ZR_ASSERT_EQ_U32(profile.cell_height_px, 20u);
  ZR_ASSERT_EQ_U32(profile.cell_width_px, 10u);
  ZR_ASSERT_EQ_U32(out_caps.supports_sync_update, 1u);

  zr_test_close_mock_platform(plat);
}

ZR_TEST_UNIT(detect_profile_known_terminal_foot) {
  mock_plat_reset();

  plat_t* plat = NULL;
  plat_caps_t baseline;
  ZR_ASSERT_EQ_U32(zr_test_open_mock_platform(&plat, &baseline), ZR_OK);

  static const uint8_t kResponses[] = "\x1bP>|foot(1.17.0)\x1b\\";
  ZR_ASSERT_EQ_U32(mock_plat_push_input(kResponses, sizeof(kResponses) - 1u), ZR_OK);

  zr_terminal_profile_t profile;
  plat_caps_t out_caps;
  ZR_ASSERT_EQ_U32(zr_detect_probe_terminal(plat, &baseline, &profile, &out_caps), ZR_OK);

  ZR_ASSERT_EQ_U32((uint32_t)profile.id, (uint32_t)ZR_TERM_FOOT);
  ZR_ASSERT_EQ_U32(profile.supports_kitty_graphics, 0u);
  ZR_ASSERT_EQ_U32(profile.supports_hyperlinks, 1u);

  zr_test_close_mock_platform(plat);
}

ZR_TEST_UNIT(detect_profile_unknown_terminal_is_conservative) {
  mock_plat_reset();

  plat_t* plat = NULL;
  plat_caps_t baseline;
  ZR_ASSERT_EQ_U32(zr_test_open_mock_platform(&plat, &baseline), ZR_OK);

  static const uint8_t kResponses[] = "\x1bP>|MyTerm 1.0\x1b\\";
  ZR_ASSERT_EQ_U32(mock_plat_push_input(kResponses, sizeof(kResponses) - 1u), ZR_OK);

  zr_terminal_profile_t profile;
  plat_caps_t out_caps;
  ZR_ASSERT_EQ_U32(zr_detect_probe_terminal(plat, &baseline, &profile, &out_caps), ZR_OK);

  ZR_ASSERT_EQ_U32((uint32_t)profile.id, (uint32_t)ZR_TERM_UNKNOWN);
  ZR_ASSERT_EQ_U32(profile.supports_kitty_graphics, 0u);
  ZR_ASSERT_EQ_U32(profile.supports_iterm2_images, 0u);
  ZR_ASSERT_EQ_U32(profile.supports_sixel, 0u);

  zr_test_close_mock_platform(plat);
}

ZR_TEST_UNIT(detect_profile_fallback_from_env_hint) {
  mock_plat_reset();
  mock_plat_set_terminal_id_hint(ZR_TERM_WEZTERM);

  plat_t* plat = NULL;
  plat_caps_t baseline;
  ZR_ASSERT_EQ_U32(zr_test_open_mock_platform(&plat, &baseline), ZR_OK);

  zr_terminal_profile_t profile;
  plat_caps_t out_caps;
  ZR_ASSERT_EQ_U32(zr_detect_probe_terminal(plat, &baseline, &profile, &out_caps), ZR_OK);

  ZR_ASSERT_EQ_U32(profile.xtversion_responded, 0u);
  ZR_ASSERT_EQ_U32((uint32_t)profile.id, (uint32_t)ZR_TERM_WEZTERM);

  zr_test_close_mock_platform(plat);
}

ZR_TEST_UNIT(detect_profile_no_env_hint_stays_unknown) {
  mock_plat_reset();
  mock_plat_set_terminal_id_hint(ZR_TERM_UNKNOWN);

  plat_t* plat = NULL;
  plat_caps_t baseline;
  ZR_ASSERT_EQ_U32(zr_test_open_mock_platform(&plat, &baseline), ZR_OK);

  zr_terminal_profile_t profile;
  plat_caps_t out_caps;
  ZR_ASSERT_EQ_U32(zr_detect_probe_terminal(plat, &baseline, &profile, &out_caps), ZR_OK);

  ZR_ASSERT_EQ_U32(profile.xtversion_responded, 0u);
  ZR_ASSERT_EQ_U32((uint32_t)profile.id, (uint32_t)ZR_TERM_UNKNOWN);

  zr_test_close_mock_platform(plat);
}

ZR_TEST_UNIT(detect_profile_skips_queries_when_unsupported) {
  mock_plat_reset();
  mock_plat_set_terminal_query_support(0u);
  mock_plat_set_terminal_id_hint(ZR_TERM_KITTY);

  plat_t* plat = NULL;
  plat_caps_t baseline;
  ZR_ASSERT_EQ_U32(zr_test_open_mock_platform(&plat, &baseline), ZR_OK);

  zr_terminal_profile_t profile;
  plat_caps_t out_caps;
  ZR_ASSERT_EQ_U32(zr_detect_probe_terminal(plat, &baseline, &profile, &out_caps), ZR_OK);

  ZR_ASSERT_EQ_U32(profile.xtversion_responded, 0u);
  ZR_ASSERT_EQ_U32((uint32_t)profile.id, (uint32_t)ZR_TERM_UNKNOWN);
  ZR_ASSERT_EQ_U32((uint32_t)out_caps.supports_sync_update, (uint32_t)baseline.supports_sync_update);

  zr_test_close_mock_platform(plat);
}
