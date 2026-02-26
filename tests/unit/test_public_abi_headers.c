/*
  tests/unit/test_public_abi_headers.c — Sanity compile/link for public ABI headers.

  Why: Ensures public ABI headers compile in isolation and the skeleton
  symbols link, without depending on engine internals.
*/

#include "zr_test.h"

#include "zr/zr_engine.h"
#include "zr/zr_event.h"
#include "zr/zr_drawlist.h"
#include "zr/zr_version.h"

#include "unit/mock_platform.h"

ZR_TEST_UNIT(public_abi_headers_compile_and_link) {
  mock_plat_reset();
  mock_plat_set_size(80u, 24u);

  zr_engine_config_t cfg = zr_engine_config_default();
  ZR_ASSERT_TRUE(zr_engine_config_validate(&cfg) == ZR_OK);

  /* Ensure the pinned version macros are usable from the public surface. */
  ZR_ASSERT_TRUE(ZR_ENGINE_ABI_MAJOR == 1u);
  ZR_ASSERT_TRUE(ZR_DRAWLIST_VERSION_V1 == 1u);
  ZR_ASSERT_TRUE(ZR_DRAWLIST_VERSION_V2 == 2u);
  ZR_ASSERT_TRUE(ZR_EVENT_BATCH_VERSION_V1 == 1u);

  /* Ensure the public engine symbols link and are callable. */
  zr_engine_t* e = NULL;
  zr_result_t rc = engine_create(&e, &cfg);
  ZR_ASSERT_TRUE(rc == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_engine_runtime_config_t r = {0};
  r.limits = cfg.limits;
  r.plat = cfg.plat;
  r.tab_width = cfg.tab_width;
  r.width_policy = cfg.width_policy;
  r.target_fps = cfg.target_fps;
  r.enable_scroll_optimizations = cfg.enable_scroll_optimizations;
  r.enable_debug_overlay = cfg.enable_debug_overlay;
  r.enable_replay_recording = cfg.enable_replay_recording;
  r.wait_for_output_drain = cfg.wait_for_output_drain;
  r.cap_force_flags = cfg.cap_force_flags;
  r.cap_suppress_flags = cfg.cap_suppress_flags;

  ZR_ASSERT_TRUE(zr_engine_runtime_config_validate(&r) == ZR_OK);

  /* Touch a couple of ABI structs to keep headers honest about types. */
  zr_evbatch_header_t h;
  h.magic = ZR_EV_MAGIC;
  h.version = ZR_EVENT_BATCH_VERSION_V1;
  h.total_size = 0u;
  h.event_count = 0u;
  h.flags = 0u;
  h.reserved0 = 0u;

  zr_dl_header_t dl;
  dl.magic = 0u;
  dl.version = ZR_DRAWLIST_VERSION_V1;
  dl.header_size = 0u;
  dl.total_size = 0u;
  dl.cmd_offset = 0u;
  dl.cmd_bytes = 0u;
  dl.cmd_count = 0u;
  dl.strings_span_offset = 0u;
  dl.strings_count = 0u;
  dl.strings_bytes_offset = 0u;
  dl.strings_bytes_len = 0u;
  dl.blobs_span_offset = 0u;
  dl.blobs_count = 0u;
  dl.blobs_bytes_offset = 0u;
  dl.blobs_bytes_len = 0u;
  dl.reserved0 = 0u;
  (void)h;
  (void)dl;

  zr_dl_cmd_draw_canvas_t canvas;
  canvas.dst_col = 0u;
  canvas.dst_row = 0u;
  canvas.dst_cols = 1u;
  canvas.dst_rows = 1u;
  canvas.px_width = 1u;
  canvas.px_height = 1u;
  canvas.blob_id = 1u;
  canvas.reserved0 = 0u;
  canvas.blitter = (uint8_t)ZR_BLIT_ASCII;
  canvas.flags = 0u;
  canvas.reserved = 0u;
  (void)canvas;

  zr_terminal_caps_t caps;
  ZR_ASSERT_TRUE(engine_get_caps(e, &caps) == ZR_OK);

  const zr_terminal_profile_t* profile = engine_get_terminal_profile(e);
  ZR_ASSERT_TRUE(profile != NULL);

  engine_destroy(e);
}
