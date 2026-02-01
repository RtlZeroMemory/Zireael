/*
  tests/unit/test_metrics_struct.c — Metrics ABI struct and prefix-copy semantics.

  Why: Ensures zr_metrics_t remains ABI-safe (POD, fixed-width fields) and
  engine_get_metrics performs bounded prefix-copy based on struct_size.
*/

#include "zr_test.h"

#include "core/zr_engine.h"
#include "core/zr_metrics_internal.h"

#include <stddef.h>
#include <string.h>

#define ZR_TEST_ASSERT_U32_FIELD_(field)                                                        \
  _Static_assert(_Generic(((zr_metrics_t*)0)->field, uint32_t : 1, default : 0),                  \
                 "zr_metrics_t field must be uint32_t")

#define ZR_TEST_ASSERT_U64_FIELD_(field)                                                        \
  _Static_assert(_Generic(((zr_metrics_t*)0)->field, uint64_t : 1, default : 0),                  \
                 "zr_metrics_t field must be uint64_t")

ZR_TEST_ASSERT_U32_FIELD_(struct_size);
ZR_TEST_ASSERT_U32_FIELD_(negotiated_engine_abi_major);
ZR_TEST_ASSERT_U32_FIELD_(negotiated_engine_abi_minor);
ZR_TEST_ASSERT_U32_FIELD_(negotiated_engine_abi_patch);
ZR_TEST_ASSERT_U32_FIELD_(negotiated_drawlist_version);
ZR_TEST_ASSERT_U32_FIELD_(negotiated_event_batch_version);
ZR_TEST_ASSERT_U64_FIELD_(frame_index);
ZR_TEST_ASSERT_U32_FIELD_(fps);
ZR_TEST_ASSERT_U64_FIELD_(bytes_emitted_total);
ZR_TEST_ASSERT_U32_FIELD_(bytes_emitted_last_frame);
ZR_TEST_ASSERT_U32_FIELD_(dirty_lines_last_frame);
ZR_TEST_ASSERT_U32_FIELD_(dirty_cols_last_frame);
ZR_TEST_ASSERT_U32_FIELD_(us_input_last_frame);
ZR_TEST_ASSERT_U32_FIELD_(us_drawlist_last_frame);
ZR_TEST_ASSERT_U32_FIELD_(us_diff_last_frame);
ZR_TEST_ASSERT_U32_FIELD_(us_write_last_frame);
ZR_TEST_ASSERT_U32_FIELD_(events_out_last_poll);
ZR_TEST_ASSERT_U32_FIELD_(events_dropped_total);
ZR_TEST_ASSERT_U64_FIELD_(arena_frame_high_water_bytes);
ZR_TEST_ASSERT_U64_FIELD_(arena_persistent_high_water_bytes);

#undef ZR_TEST_ASSERT_U32_FIELD_
#undef ZR_TEST_ASSERT_U64_FIELD_

static zr_engine_t* zr_dummy_engine_ptr(void) {
  static uint8_t dummy;
  return (zr_engine_t*)&dummy;
}

ZR_TEST_UNIT(metrics_prefix_copy_full_size_copies_all_fields) {
  zr_metrics_t snap = zr_metrics__default_snapshot();
  snap.negotiated_engine_abi_major = 9u;
  snap.negotiated_engine_abi_minor = 8u;
  snap.negotiated_engine_abi_patch = 7u;
  snap.negotiated_drawlist_version = 11u;
  snap.negotiated_event_batch_version = 12u;
  snap.frame_index = 123u;
  snap.fps = 60u;
  snap.bytes_emitted_total = 0x1122334455667788ull;
  snap.bytes_emitted_last_frame = 1234u;
  snap.dirty_lines_last_frame = 2u;
  snap.dirty_cols_last_frame = 7u;
  snap.us_input_last_frame = 1u;
  snap.us_drawlist_last_frame = 2u;
  snap.us_diff_last_frame = 3u;
  snap.us_write_last_frame = 4u;
  snap.events_out_last_poll = 5u;
  snap.events_dropped_total = 6u;
  snap.arena_frame_high_water_bytes = 77ull;
  snap.arena_persistent_high_water_bytes = 88ull;

  zr_metrics_t out;
  memset(&out, 0xCC, sizeof(out));
  out.struct_size = (uint32_t)sizeof(zr_metrics_t);

  zr_result_t rc = zr_metrics__copy_out(&out, &snap);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  ZR_ASSERT_EQ_U32(out.struct_size, (uint32_t)sizeof(zr_metrics_t));
  ZR_ASSERT_EQ_U32(out.negotiated_engine_abi_major, 9u);
  ZR_ASSERT_EQ_U32(out.negotiated_engine_abi_minor, 8u);
  ZR_ASSERT_EQ_U32(out.negotiated_engine_abi_patch, 7u);
  ZR_ASSERT_EQ_U32(out.negotiated_drawlist_version, 11u);
  ZR_ASSERT_EQ_U32(out.negotiated_event_batch_version, 12u);
  ZR_ASSERT_EQ_U32(out.fps, 60u);
  ZR_ASSERT_EQ_U32(out.bytes_emitted_last_frame, 1234u);
  ZR_ASSERT_EQ_U32(out.dirty_lines_last_frame, 2u);
  ZR_ASSERT_EQ_U32(out.dirty_cols_last_frame, 7u);
  ZR_ASSERT_EQ_U32(out.us_input_last_frame, 1u);
  ZR_ASSERT_EQ_U32(out.us_drawlist_last_frame, 2u);
  ZR_ASSERT_EQ_U32(out.us_diff_last_frame, 3u);
  ZR_ASSERT_EQ_U32(out.us_write_last_frame, 4u);
  ZR_ASSERT_EQ_U32(out.events_out_last_poll, 5u);
  ZR_ASSERT_EQ_U32(out.events_dropped_total, 6u);
  ZR_ASSERT_TRUE(out.frame_index == 123u);
  ZR_ASSERT_TRUE(out.bytes_emitted_total == 0x1122334455667788ull);
  ZR_ASSERT_TRUE(out.arena_frame_high_water_bytes == 77ull);
  ZR_ASSERT_TRUE(out.arena_persistent_high_water_bytes == 88ull);
}

ZR_TEST_UNIT(metrics_prefix_copy_smaller_struct_size_does_not_overrun) {
  zr_metrics_t snap = zr_metrics__default_snapshot();
  snap.negotiated_engine_abi_major = 1u;
  snap.negotiated_engine_abi_minor = 2u;
  snap.negotiated_engine_abi_patch = 3u;
  snap.negotiated_drawlist_version = 4u;
  snap.negotiated_event_batch_version = 5u;
  snap.frame_index = 99u;

  zr_metrics_t out;
  memset(&out, 0xA5, sizeof(out));

  const size_t prefix = offsetof(zr_metrics_t, bytes_emitted_total);
  ZR_ASSERT_TRUE(prefix > 0u);
  ZR_ASSERT_TRUE(prefix < sizeof(zr_metrics_t));
  out.struct_size = (uint32_t)prefix;

  zr_result_t rc = zr_metrics__copy_out(&out, &snap);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  const zr_metrics_t expected = (zr_metrics_t){
      .struct_size = (uint32_t)sizeof(zr_metrics_t),
      .negotiated_engine_abi_major = 1u,
      .negotiated_engine_abi_minor = 2u,
      .negotiated_engine_abi_patch = 3u,
      .negotiated_drawlist_version = 4u,
      .negotiated_event_batch_version = 5u,
      .frame_index = 99u,
  };

  ZR_ASSERT_MEMEQ(&out, &expected, prefix);

  const uint8_t* tail = (const uint8_t*)&out + prefix;
  for (size_t i = 0u; i < sizeof(zr_metrics_t) - prefix; i++) {
    ZR_ASSERT_TRUE(tail[i] == 0xA5u);
  }
}

ZR_TEST_UNIT(metrics_prefix_copy_zero_struct_size_writes_nothing) {
  const zr_metrics_t snap = zr_metrics__default_snapshot();

  zr_metrics_t out;
  memset(&out, 0xCC, sizeof(out));
  out.struct_size = 0u;
  const zr_metrics_t before = out;

  ZR_ASSERT_EQ_U32(zr_metrics__copy_out(&out, &snap), ZR_OK);

  ZR_ASSERT_MEMEQ(&out, &before, sizeof(out));
}

ZR_TEST_UNIT(engine_get_metrics_uses_prefix_copy_contract) {
  zr_metrics_t out;
  memset(&out, 0, sizeof(out));
  out.struct_size = (uint32_t)sizeof(zr_metrics_t);

  ZR_ASSERT_EQ_U32(engine_get_metrics(zr_dummy_engine_ptr(), &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out.struct_size, (uint32_t)sizeof(zr_metrics_t));
  ZR_ASSERT_EQ_U32(out.negotiated_engine_abi_major, 1u);
  ZR_ASSERT_EQ_U32(out.negotiated_drawlist_version, 1u);
  ZR_ASSERT_EQ_U32(out.negotiated_event_batch_version, 1u);
}
