/*
  tests/unit/test_engine_present_single_flush.c — Engine present single-flush contract.

  Why: Validates that engine_present emits terminal bytes via exactly one
  plat_write_output call on success, and does not flush at all when diff output
  cannot fit in the engine-owned per-frame output buffer.
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"

#include "unit/mock_platform.h"

#include <stdbool.h>
#include <string.h>

extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;

static bool zr_contains_bytes(const uint8_t* hay, size_t hay_len, const uint8_t* needle, size_t needle_len) {
  if (!hay || !needle || needle_len == 0u || needle_len > hay_len) {
    return false;
  }
  for (size_t i = 0u; i + needle_len <= hay_len; i++) {
    if (memcmp(hay + i, needle, needle_len) == 0) {
      return true;
    }
  }
  return false;
}

ZR_TEST_UNIT(engine_present_single_flush_on_success) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(engine_submit_drawlist(e, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len) == ZR_OK);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_OK);

  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);
  ZR_ASSERT_TRUE(mock_plat_bytes_written_total() != 0u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_emits_debug_overlay_when_enabled) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.enable_debug_overlay = 1u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_OK);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);

  uint8_t out[8192];
  const size_t out_len = mock_plat_last_write_copy(out, sizeof(out));
  static const uint8_t needle[] = "FPS:";
  ZR_ASSERT_TRUE(zr_contains_bytes(out, out_len, needle, sizeof(needle) - 1u));

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_does_not_emit_debug_overlay_when_disabled) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.enable_debug_overlay = 0u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_OK);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);

  uint8_t out[8192];
  const size_t out_len = mock_plat_last_write_copy(out, sizeof(out));
  static const uint8_t needle[] = "FPS:";
  ZR_ASSERT_TRUE(!zr_contains_bytes(out, out_len, needle, sizeof(needle) - 1u));

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_sync_update_overhead_does_not_force_limit) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_mouse = 0u;
  caps.supports_bracketed_paste = 1u;
  caps.supports_focus_events = 1u;
  caps.supports_osc52 = 0u;
  caps.supports_sync_update = 1u;
  caps.supports_scroll_region = 1u;
  caps.supports_cursor_shape = 0u;
  caps.supports_output_wait_writable = 0u;
  caps._pad0[0] = 0u;
  caps._pad0[1] = 0u;
  caps._pad0[2] = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;
  mock_plat_set_caps(caps);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 8u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_OK);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_wraps_output_with_sync_update_when_supported) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_mouse = 0u;
  caps.supports_bracketed_paste = 1u;
  caps.supports_focus_events = 1u;
  caps.supports_osc52 = 0u;
  caps.supports_sync_update = 1u;
  caps.supports_scroll_region = 1u;
  caps.supports_cursor_shape = 0u;
  caps.supports_output_wait_writable = 0u;
  caps._pad0[0] = 0u;
  caps._pad0[1] = 0u;
  caps._pad0[2] = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;
  mock_plat_set_caps(caps);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(engine_submit_drawlist(e, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len) == ZR_OK);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_OK);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);

  static const uint8_t sync_begin[] = "\x1b[?2026h";
  static const uint8_t sync_end[] = "\x1b[?2026l";

  uint8_t out[8192];
  const size_t out_len = mock_plat_last_write_copy(out, sizeof(out));
  ZR_ASSERT_TRUE(out_len >= (sizeof(sync_begin) - 1u) + (sizeof(sync_end) - 1u));

  ZR_ASSERT_TRUE(memcmp(out, sync_begin, sizeof(sync_begin) - 1u) == 0);
  ZR_ASSERT_TRUE(memcmp(out + out_len - (sizeof(sync_end) - 1u), sync_end, sizeof(sync_end) - 1u) == 0);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_no_flush_on_limit_error) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 8u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(engine_submit_drawlist(e, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len) == ZR_OK);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 0u);

  engine_destroy(e);
}
