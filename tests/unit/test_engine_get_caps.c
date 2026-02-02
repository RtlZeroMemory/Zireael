/*
  tests/unit/test_engine_get_caps.c — Unit tests for engine_get_caps (public ABI).

  Why: Ensures wrappers can query the engine’s runtime capability snapshot
  deterministically via the public API.
*/

#include "zr_test.h"

#include "core/zr_engine.h"

#include "unit/mock_platform.h"

#include <string.h>

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
  caps._pad0[0] = 0u;
  caps._pad0[1] = 0u;
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

