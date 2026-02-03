/*
  tests/unit/test_engine_tick_events.c — Engine tick event emission.

  Why: Ensures the engine emits ZR_EV_TICK periodically from engine_poll_events()
  (even without input) and that tick.dt_ms is non-zero and bounded by target_fps.
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"
#include "core/zr_event.h"

#include "unit/mock_platform.h"

#include "util/zr_bytes.h"

#include <stddef.h>
#include <string.h>

static size_t zr_align4(size_t v) { return (v + 3u) & ~(size_t)3u; }

static uint32_t zr_u32le_at(const uint8_t* p) { return zr_load_u32le(p); }

ZR_TEST_UNIT(engine_poll_events_emits_tick_with_nonzero_dt) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u; /* 50ms interval */
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  uint8_t out[128];
  memset(out, 0, sizeof(out));

  /* engine_create() enqueues an initial resize event. Drain it first. */
  {
    const int n0 = engine_poll_events(e, 0, out, (int)sizeof(out));
    ZR_ASSERT_TRUE(n0 > 0);

    ZR_ASSERT_EQ_U32(zr_u32le_at(out + 0u), ZR_EV_MAGIC);
    ZR_ASSERT_EQ_U32(zr_u32le_at(out + 4u), ZR_EVENT_BATCH_VERSION_V1);
    ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u); /* event_count */

    const size_t off_rec0 = sizeof(zr_evbatch_header_t);
    ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_RESIZE);
  }

  /* Immediately after create: no tick yet (dt would be 0). */
  memset(out, 0, sizeof(out));
  ZR_ASSERT_TRUE(engine_poll_events(e, 0, out, (int)sizeof(out)) == 0);

  /* Advance time past the configured tick interval and poll again. */
  mock_plat_set_now_ms(1050u);
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 0u), ZR_EV_MAGIC);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 4u), ZR_EVENT_BATCH_VERSION_V1);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u); /* event_count */

  /* Record 0 header starts immediately after the batch header. */
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_TICK);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 8u), 1050u); /* time_ms */

  /* Tick payload begins after zr_ev_record_header_t. */
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  const uint32_t dt_ms = zr_u32le_at(out + off_payload + 0u);
  ZR_ASSERT_TRUE(dt_ms > 0u);
  ZR_ASSERT_EQ_U32(dt_ms, 50u);

  /* No event spam: polling again at same time produces no new tick. */
  memset(out, 0, sizeof(out));
  ZR_ASSERT_TRUE(engine_poll_events(e, 0, out, (int)sizeof(out)) == 0);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_drains_input_before_due_tick) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u; /* 50ms interval */
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  /* Drain initial resize event enqueued by engine_create(). */
  {
    uint8_t out0[128];
    memset(out0, 0, sizeof(out0));
    const int n0 = engine_poll_events(e, 0, out0, (int)sizeof(out0));
    ZR_ASSERT_TRUE(n0 > 0);
  }

  mock_plat_set_now_ms(1050u);

  /* One key event: ESC [ A (UP). */
  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'A'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 0u), ZR_EV_MAGIC);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 4u), ZR_EVENT_BATCH_VERSION_V1);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 2u); /* event_count */

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);

  const size_t rec0_bytes = zr_align4(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t));
  const size_t off_rec1 = off_rec0 + rec0_bytes;
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec1 + 0u), (uint32_t)ZR_EV_TICK);

  engine_destroy(e);
}
