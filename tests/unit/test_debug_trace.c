/*
  tests/unit/test_debug_trace.c — Unit tests for debug trace ring buffer.

  Why: Verifies the debug trace system correctly captures, stores, queries,
  and exports diagnostic records with proper ring buffer semantics.
*/

#include "zr_test.h"

#include "core/zr_debug_trace.h"

#include <string.h>

/* --- Test storage --- */

enum {
  TEST_RING_BUF_SIZE = 4096u,
  TEST_INDEX_CAP = 64u,
};

static uint8_t g_ring_buf[TEST_RING_BUF_SIZE];
static uint32_t g_record_offsets[TEST_INDEX_CAP];
static uint32_t g_record_sizes[TEST_INDEX_CAP];

/* --- Helper to init trace with test storage --- */

static zr_result_t test_trace_init(zr_debug_trace_t* t, const zr_debug_config_t* config) {
  memset(g_ring_buf, 0, sizeof(g_ring_buf));
  memset(g_record_offsets, 0, sizeof(g_record_offsets));
  memset(g_record_sizes, 0, sizeof(g_record_sizes));

  return zr_debug_trace_init(t, config, g_ring_buf, TEST_RING_BUF_SIZE, g_record_offsets, g_record_sizes,
                             TEST_INDEX_CAP);
}

/* Simulated timestamp for testing. */
static uint64_t test_timestamp_us(void) {
  return 1000000u; /* 1 second in microseconds */
}

/* --- Tests --- */

ZR_TEST(unit_debug_trace_init_disabled) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 0u;

  zr_result_t rc = zr_debug_trace_init(&t, &cfg, NULL, 0, NULL, NULL, 0);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Disabled trace should not require storage. */
  ZR_ASSERT_TRUE(!zr_debug_trace_enabled(&t, ZR_DEBUG_CAT_FRAME, ZR_DEBUG_SEV_INFO));
}

ZR_TEST(unit_debug_trace_init_enabled) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  ZR_ASSERT_TRUE(zr_debug_trace_enabled(&t, ZR_DEBUG_CAT_FRAME, ZR_DEBUG_SEV_INFO));
  ZR_ASSERT_TRUE(!zr_debug_trace_enabled(&t, ZR_DEBUG_CAT_FRAME, ZR_DEBUG_SEV_TRACE));
}

ZR_TEST(unit_debug_trace_init_null_storage_fails) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;

  /* Enabled trace with NULL storage should fail. */
  zr_result_t rc = zr_debug_trace_init(&t, &cfg, NULL, 0, NULL, NULL, 0);
  ZR_ASSERT_EQ_U32(rc, ZR_ERR_INVALID_ARGUMENT);
}

ZR_TEST(unit_debug_trace_record_basic) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  zr_debug_trace_set_frame(&t, 42u);
  zr_debug_trace_set_start_time(&t, 500000u); /* 0.5 seconds */

  /* Record a simple frame event. */
  zr_debug_frame_record_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.frame_id = 42u;
  frame.cols = 80u;
  frame.rows = 24u;
  frame.diff_bytes_emitted = 1024u;

  rc = zr_debug_trace_frame(&t, ZR_DEBUG_CODE_FRAME_PRESENT, test_timestamp_us(), &frame);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Query the record. */
  zr_debug_query_t query;
  memset(&query, 0, sizeof(query));
  query.category_mask = 0xFFFFFFFFu;
  query.max_records = 10u;

  zr_debug_record_header_t headers[10];
  zr_debug_query_result_t result;

  rc = zr_debug_trace_query(&t, &query, headers, 10u, &result);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(result.records_returned, 1u);
  ZR_ASSERT_EQ_U32(result.records_available, 1u);

  ZR_ASSERT_EQ_U32(headers[0].category, (uint32_t)ZR_DEBUG_CAT_FRAME);
  ZR_ASSERT_EQ_U32((uint32_t)headers[0].frame_id, 42u);
  ZR_ASSERT_EQ_U32(headers[0].code, (uint32_t)ZR_DEBUG_CODE_FRAME_PRESENT);

  /* Verify relative timestamp (1000000 - 500000 = 500000). */
  ZR_ASSERT_EQ_U32((uint32_t)headers[0].timestamp_us, 500000u);
}

ZR_TEST(unit_debug_trace_record_filtered) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_WARN; /* Only warnings and errors. */

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* INFO should be filtered out. */
  ZR_ASSERT_TRUE(!zr_debug_trace_enabled(&t, ZR_DEBUG_CAT_FRAME, ZR_DEBUG_SEV_INFO));
  ZR_ASSERT_TRUE(zr_debug_trace_enabled(&t, ZR_DEBUG_CAT_FRAME, ZR_DEBUG_SEV_WARN));

  /* Record an INFO event (should be silently ignored). */
  zr_debug_frame_record_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.frame_id = 1u;

  rc = zr_debug_trace_frame(&t, ZR_DEBUG_CODE_FRAME_PRESENT, test_timestamp_us(), &frame);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Query should return no records. */
  zr_debug_query_t query;
  memset(&query, 0, sizeof(query));
  query.category_mask = 0xFFFFFFFFu;
  query.max_records = 10u;

  zr_debug_record_header_t headers[10];
  zr_debug_query_result_t result;

  rc = zr_debug_trace_query(&t, &query, headers, 10u, &result);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(result.records_returned, 0u);
}

ZR_TEST(unit_debug_trace_get_payload) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  zr_debug_frame_record_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.frame_id = 99u;
  frame.cols = 120u;
  frame.rows = 40u;

  rc = zr_debug_trace_frame(&t, ZR_DEBUG_CODE_FRAME_PRESENT, test_timestamp_us(), &frame);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Query to get record ID. */
  zr_debug_query_t query;
  memset(&query, 0, sizeof(query));
  query.category_mask = 0xFFFFFFFFu;
  query.max_records = 1u;

  zr_debug_record_header_t headers[1];
  zr_debug_query_result_t result;

  rc = zr_debug_trace_query(&t, &query, headers, 1u, &result);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(result.records_returned, 1u);

  /* Get payload. */
  zr_debug_frame_record_t payload;
  uint32_t payload_size = 0u;

  rc = zr_debug_trace_get_payload(&t, headers[0].record_id, &payload, sizeof(payload), &payload_size);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(payload_size, sizeof(zr_debug_frame_record_t));
  ZR_ASSERT_EQ_U32((uint32_t)payload.frame_id, 99u);
  ZR_ASSERT_EQ_U32(payload.cols, 120u);
  ZR_ASSERT_EQ_U32(payload.rows, 40u);
}

ZR_TEST(unit_debug_trace_ring_overflow) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Fill the ring buffer beyond capacity. */
  zr_debug_frame_record_t frame;
  memset(&frame, 0, sizeof(frame));

  for (uint32_t i = 0u; i < TEST_INDEX_CAP + 10u; i++) {
    frame.frame_id = i;
    rc = zr_debug_trace_frame(&t, ZR_DEBUG_CODE_FRAME_PRESENT, test_timestamp_us(), &frame);
    ZR_ASSERT_EQ_U32(rc, ZR_OK);
  }

  /* Stats should show drops. */
  zr_debug_stats_t stats;
  rc = zr_debug_trace_get_stats(&t, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_TRUE(stats.total_dropped > 0u);
  ZR_ASSERT_TRUE(stats.current_ring_usage <= TEST_INDEX_CAP);
}

ZR_TEST(unit_debug_trace_index_overflow_without_byte_overflow) {
  (void)ctx;

  enum { BIG_RING_BUF_SIZE = 64u * 1024u, SMALL_INDEX_CAP = 8u };

  uint8_t ring_buf[BIG_RING_BUF_SIZE];
  uint32_t record_offsets[SMALL_INDEX_CAP];
  uint32_t record_sizes[SMALL_INDEX_CAP];

  memset(ring_buf, 0, sizeof(ring_buf));
  memset(record_offsets, 0, sizeof(record_offsets));
  memset(record_sizes, 0, sizeof(record_sizes));

  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;

  zr_result_t rc =
      zr_debug_trace_init(&t, &cfg, ring_buf, sizeof(ring_buf), record_offsets, record_sizes, SMALL_INDEX_CAP);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  zr_debug_trace_set_start_time(&t, 0u);
  zr_debug_trace_set_frame(&t, 1u);

  zr_debug_perf_record_t perf;
  memset(&perf, 0, sizeof(perf));
  perf.frame_id = 1u;
  perf.phase = 2u;
  perf.us_elapsed = 123u;
  perf.bytes_processed = 456u;

  /* Write far more records than index capacity without exhausting byte storage. */
  for (uint32_t i = 0u; i < 32u; i++) {
    perf.us_elapsed = i;
    rc = zr_debug_trace_perf(&t, test_timestamp_us(), &perf);
    ZR_ASSERT_EQ_U32(rc, ZR_OK);
  }

  zr_debug_stats_t stats;
  rc = zr_debug_trace_get_stats(&t, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(stats.current_ring_usage, (uint32_t)SMALL_INDEX_CAP);
  ZR_ASSERT_TRUE(stats.total_dropped > 0u);

  zr_debug_query_t query;
  memset(&query, 0, sizeof(query));
  query.category_mask = 0xFFFFFFFFu;
  query.min_severity = ZR_DEBUG_SEV_TRACE;
  query.max_records = 32u;

  zr_debug_record_header_t headers[32];
  zr_debug_query_result_t result;
  rc = zr_debug_trace_query(&t, &query, headers, 32u, &result);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(result.records_returned, (uint32_t)SMALL_INDEX_CAP);

  /* Record IDs returned newest-to-oldest; ensure they are strictly decreasing. */
  for (uint32_t i = 1u; i < result.records_returned; i++) {
    ZR_ASSERT_TRUE(headers[i - 1u].record_id > headers[i].record_id);
  }

  /* Oldest returned record should still be fetchable. */
  zr_debug_perf_record_t payload;
  uint32_t payload_size = 0u;
  rc = zr_debug_trace_get_payload(&t, headers[result.records_returned - 1u].record_id, &payload, sizeof(payload),
                                  &payload_size);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(payload_size, (uint32_t)sizeof(zr_debug_perf_record_t));
}

ZR_TEST(unit_debug_trace_query_filter_frame) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Record events for different frames. */
  zr_debug_frame_record_t frame;
  memset(&frame, 0, sizeof(frame));

  for (uint32_t i = 1u; i <= 5u; i++) {
    zr_debug_trace_set_frame(&t, i);
    frame.frame_id = i;
    rc = zr_debug_trace_frame(&t, ZR_DEBUG_CODE_FRAME_PRESENT, test_timestamp_us(), &frame);
    ZR_ASSERT_EQ_U32(rc, ZR_OK);
  }

  /* Query only frames 2-4. */
  zr_debug_query_t query;
  memset(&query, 0, sizeof(query));
  query.category_mask = 0xFFFFFFFFu;
  query.min_frame_id = 2u;
  query.max_frame_id = 4u;
  query.max_records = 10u;

  zr_debug_record_header_t headers[10];
  zr_debug_query_result_t result;

  rc = zr_debug_trace_query(&t, &query, headers, 10u, &result);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(result.records_available, 3u);
}

ZR_TEST(unit_debug_trace_query_saturates_records_dropped_u32) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  t.total_dropped = UINT64_MAX;

  zr_debug_query_t query;
  memset(&query, 0, sizeof(query));
  query.category_mask = 0xFFFFFFFFu;
  query.max_records = 1u;

  zr_debug_record_header_t headers[1];
  zr_debug_query_result_t result;
  rc = zr_debug_trace_query(&t, &query, headers, 1u, &result);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(result.records_dropped, UINT32_MAX);
}

ZR_TEST(unit_debug_trace_reset) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Add some records. */
  zr_debug_frame_record_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.frame_id = 1u;

  rc = zr_debug_trace_frame(&t, ZR_DEBUG_CODE_FRAME_PRESENT, test_timestamp_us(), &frame);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Reset. */
  zr_debug_trace_reset(&t);

  /* Query should return no records. */
  zr_debug_query_t query;
  memset(&query, 0, sizeof(query));
  query.category_mask = 0xFFFFFFFFu;
  query.max_records = 10u;

  zr_debug_record_header_t headers[10];
  zr_debug_query_result_t result;

  rc = zr_debug_trace_query(&t, &query, headers, 10u, &result);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(result.records_returned, 0u);
}

ZR_TEST(unit_debug_trace_export) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Add a few records. */
  zr_debug_frame_record_t frame;
  memset(&frame, 0, sizeof(frame));

  for (uint32_t i = 1u; i <= 3u; i++) {
    frame.frame_id = i;
    rc = zr_debug_trace_frame(&t, ZR_DEBUG_CODE_FRAME_PRESENT, test_timestamp_us(), &frame);
    ZR_ASSERT_EQ_U32(rc, ZR_OK);
  }

  /* Export to buffer. */
  uint8_t export_buf[2048];
  int32_t exported = zr_debug_trace_export(&t, export_buf, sizeof(export_buf));

  ZR_ASSERT_TRUE(exported > 0);

  /* Exported data should contain 3 records. */
  const size_t expected_size = 3u * (sizeof(zr_debug_record_header_t) + sizeof(zr_debug_frame_record_t));
  ZR_ASSERT_EQ_U32((uint32_t)exported, (uint32_t)expected_size);
}

ZR_TEST(unit_debug_trace_error_record) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Record an error. */
  zr_debug_error_record_t error;
  memset(&error, 0, sizeof(error));
  error.frame_id = 10u;
  error.error_code = (uint32_t)ZR_ERR_FORMAT;
  error.occurrence_count = 1u;

  const char* src_file = "test_file.c";
  const char* msg = "Test error message";
  size_t src_len = strlen(src_file);
  size_t msg_len = strlen(msg);
  if (src_len > sizeof(error.source_file) - 1u) {
    src_len = sizeof(error.source_file) - 1u;
  }
  if (msg_len > sizeof(error.message) - 1u) {
    msg_len = sizeof(error.message) - 1u;
  }
  memcpy(error.source_file, src_file, src_len);
  memcpy(error.message, msg, msg_len);

  rc = zr_debug_trace_error(&t, ZR_DEBUG_CODE_ERROR_GENERIC, test_timestamp_us(), &error);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Stats should show one error. */
  zr_debug_stats_t stats;
  rc = zr_debug_trace_get_stats(&t, &stats);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(stats.error_count, 1u);
}

ZR_TEST(unit_debug_trace_category_filter) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;
  cfg.category_mask = (1u << ZR_DEBUG_CAT_ERROR); /* Only errors. */

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  /* Frame category should be filtered. */
  ZR_ASSERT_TRUE(!zr_debug_trace_enabled(&t, ZR_DEBUG_CAT_FRAME, ZR_DEBUG_SEV_INFO));
  ZR_ASSERT_TRUE(zr_debug_trace_enabled(&t, ZR_DEBUG_CAT_ERROR, ZR_DEBUG_SEV_ERROR));
}

ZR_TEST(unit_debug_config_default) {
  (void)ctx;
  zr_debug_config_t cfg = zr_debug_config_default();

  ZR_ASSERT_EQ_U32(cfg.enabled, 0u);
  ZR_ASSERT_EQ_U32(cfg.ring_capacity, ZR_DEBUG_DEFAULT_RING_CAP);
  ZR_ASSERT_EQ_U32(cfg.min_severity, (uint32_t)ZR_DEBUG_SEV_INFO);
  ZR_ASSERT_EQ_U32(cfg.category_mask, 0xFFFFFFFFu);
}

ZR_TEST(unit_debug_trace_timestamp_ordering) {
  (void)ctx;
  zr_debug_trace_t t;
  zr_debug_config_t cfg = zr_debug_config_default();
  cfg.enabled = 1u;
  cfg.min_severity = ZR_DEBUG_SEV_TRACE;

  zr_result_t rc = test_trace_init(&t, &cfg);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);

  zr_debug_trace_set_start_time(&t, 0u);

  /* Record events with increasing timestamps. */
  zr_debug_frame_record_t frame;
  memset(&frame, 0, sizeof(frame));

  for (uint32_t i = 1u; i <= 5u; i++) {
    frame.frame_id = i;
    rc = zr_debug_trace_frame(&t, ZR_DEBUG_CODE_FRAME_PRESENT, i * 100000u, &frame);
    ZR_ASSERT_EQ_U32(rc, ZR_OK);
  }

  /* Query all records. */
  zr_debug_query_t query;
  memset(&query, 0, sizeof(query));
  query.category_mask = 0xFFFFFFFFu;
  query.max_records = 10u;

  zr_debug_record_header_t headers[10];
  zr_debug_query_result_t result;

  rc = zr_debug_trace_query(&t, &query, headers, 10u, &result);
  ZR_ASSERT_EQ_U32(rc, ZR_OK);
  ZR_ASSERT_EQ_U32(result.records_returned, 5u);

  /* Verify timestamps are increasing (newest to oldest in query result). */
  for (uint32_t i = 0u; i < result.records_returned - 1u; i++) {
    ZR_ASSERT_TRUE(headers[i].timestamp_us >= headers[i + 1].timestamp_us);
  }
}
