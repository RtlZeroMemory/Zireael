/*
  tests/unit/test_engine_runtime_behavior.c — Runtime config and metrics behavior.

  Why: Verifies target_fps validation, debug overlay wiring, metrics updates,
  and split-sequence input handling through engine_poll_events().
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"
#include "core/zr_event.h"

#include "unit/mock_platform.h"

#include "util/zr_bytes.h"

#include <string.h>

static zr_engine_runtime_config_t zr_runtime_from_create(const zr_engine_config_t* cfg) {
  zr_engine_runtime_config_t rcfg;
  memset(&rcfg, 0, sizeof(rcfg));
  rcfg.limits = cfg->limits;
  rcfg.plat = cfg->plat;
  rcfg.tab_width = cfg->tab_width;
  rcfg.width_policy = cfg->width_policy;
  rcfg.target_fps = cfg->target_fps;
  rcfg.enable_scroll_optimizations = cfg->enable_scroll_optimizations;
  rcfg.enable_debug_overlay = cfg->enable_debug_overlay;
  rcfg.enable_replay_recording = cfg->enable_replay_recording;
  rcfg._pad0 = 0u;
  return rcfg;
}

ZR_TEST_UNIT(engine_config_validate_rejects_invalid_target_fps) {
  zr_engine_config_t cfg = zr_engine_config_default();

  cfg.target_fps = 0u;
  ZR_ASSERT_EQ_U32(zr_engine_config_validate(&cfg), ZR_ERR_INVALID_ARGUMENT);

  cfg.target_fps = 1001u;
  ZR_ASSERT_EQ_U32(zr_engine_config_validate(&cfg), ZR_ERR_INVALID_ARGUMENT);

  cfg.target_fps = 60u;
  ZR_ASSERT_EQ_U32(zr_engine_config_validate(&cfg), ZR_OK);
}

ZR_TEST_UNIT(engine_present_updates_fps_and_arena_high_water) {
  mock_plat_reset();
  mock_plat_set_size(80u, 24u);

  zr_engine_config_t cfg = zr_engine_config_default();
  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  mock_plat_set_now_ms(1000u);
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);

  mock_plat_set_now_ms(1100u);
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);

  zr_metrics_t m;
  memset(&m, 0, sizeof(m));
  m.struct_size = (uint32_t)sizeof(m);
  ZR_ASSERT_EQ_U32(engine_get_metrics(e, &m), ZR_OK);

  ZR_ASSERT_EQ_U32(m.fps, 10u);
  ZR_ASSERT_TRUE(m.arena_frame_high_water_bytes > 0u);
  ZR_ASSERT_TRUE(m.arena_persistent_high_water_bytes > 0u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_uses_debug_overlay_flag) {
  mock_plat_reset();
  mock_plat_set_size(80u, 24u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.enable_debug_overlay = 0u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  mock_plat_clear_writes();
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);
  ZR_ASSERT_TRUE(mock_plat_bytes_written_total() == 0u);

  zr_engine_runtime_config_t rcfg = zr_runtime_from_create(&cfg);
  rcfg.enable_debug_overlay = 1u;
  ZR_ASSERT_EQ_U32(engine_set_config(e, &rcfg), ZR_OK);

  mock_plat_clear_writes();
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);
  ZR_ASSERT_TRUE(mock_plat_bytes_written_total() > 0u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_handles_split_escape_sequence) {
  mock_plat_reset();
  mock_plat_set_size(80u, 24u);

  zr_engine_config_t cfg = zr_engine_config_default();
  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const uint8_t part0[] = {0x1Bu, (uint8_t)'['};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(part0, sizeof(part0)), ZR_OK);
  ZR_ASSERT_EQ_U32((uint32_t)engine_poll_events(e, 0, out, (int)sizeof(out)), 0u);

  const uint8_t part1[] = {(uint8_t)'A'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(part1, sizeof(part1)), ZR_OK);

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_load_u32le(out + 0u), ZR_EV_MAGIC);
  ZR_ASSERT_EQ_U32(zr_load_u32le(out + 12u), 1u); /* event_count */

  const uint32_t rec_type = zr_load_u32le(out + sizeof(zr_evbatch_header_t) + 0u);
  ZR_ASSERT_EQ_U32(rec_type, (uint32_t)ZR_EV_KEY);

  const uint32_t key = zr_load_u32le(out + sizeof(zr_evbatch_header_t) + sizeof(zr_ev_record_header_t) + 0u);
  ZR_ASSERT_EQ_U32(key, (uint32_t)ZR_KEY_UP);

  engine_destroy(e);
}
