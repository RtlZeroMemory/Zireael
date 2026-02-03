/*
  tests/unit/test_engine_poll_events_truncation.c — Engine poll truncation semantics.

  Why: Validates the locked packed event batch truncation behavior:
    - If the output buffer cannot fit the batch header, engine_poll_events returns
      ZR_ERR_LIMIT and writes nothing.
    - If the output buffer fits the header but not all records, truncation is a
      success mode: TRUNCATED flag set and bytes_written returned.
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

ZR_TEST_UNIT(engine_poll_events_truncates_as_success_with_flag) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
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

  /* Two key events: ESC [ A (UP), ESC [ B (DOWN). */
  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'A', 0x1Bu, (uint8_t)'[', (uint8_t)'B'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  const size_t hdr = sizeof(zr_evbatch_header_t);
  const size_t rec = zr_align4(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t));
  const size_t cap = hdr + rec; /* fits header + exactly one key record */

  uint8_t out[128];
  ZR_ASSERT_TRUE(cap <= sizeof(out));
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)cap);
  ZR_ASSERT_TRUE(n > 0);
  ZR_ASSERT_TRUE((size_t)n == cap);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 0u), ZR_EV_MAGIC);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 4u), ZR_EVENT_BATCH_VERSION_V1);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 8u), (uint32_t)cap);       /* total_size */
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);                  /* event_count */
  ZR_ASSERT_TRUE((zr_u32le_at(out + 16u) & ZR_EV_BATCH_TRUNCATED) != 0u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_header_too_small_returns_limit_and_writes_nothing) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  /* One key event (TAB). */
  const uint8_t in[] = {(uint8_t)'\t'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  const size_t cap = sizeof(zr_evbatch_header_t) - 1u;
  uint8_t out[64];
  ZR_ASSERT_TRUE(cap <= sizeof(out));

  memset(out, 0xAA, sizeof(out));
  const uint8_t expect[64] = {
      0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
      0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
      0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
      0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
  };

  const int n = engine_poll_events(e, 0, out, (int)cap);
  ZR_ASSERT_TRUE(n == (int)ZR_ERR_LIMIT);
  ZR_ASSERT_MEMEQ(out, expect, sizeof(out));

  engine_destroy(e);
}
