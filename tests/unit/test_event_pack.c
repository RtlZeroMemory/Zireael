/*
  tests/unit/test_event_pack.c — Unit tests for packed event batch writer.

  Why: Validates the deterministic binary event batch format used for
  engine-to-caller event delivery. Tests ensure correct header layout,
  record formatting, and the "no partial record" truncation policy.

  Scenarios tested:
    - Header + single record written with correct format and alignment
    - Buffer too small for header rejects initialization
    - Truncation: sets ZR_EV_BATCH_TRUNCATED flag, no partial records written
    - Partial fit: some records written, truncation flag set for remainder
*/

#include "zr_test.h"

#include "core/zr_event_pack.h"

#include <string.h>

/* Helper macro for little-endian u32 byte expansion in expected arrays. */
#define ZR_U32LE(v)                                                                    \
  (uint8_t)((uint32_t)(v)&0xFFu), (uint8_t)(((uint32_t)(v) >> 8u) & 0xFFu),             \
      (uint8_t)(((uint32_t)(v) >> 16u) & 0xFFu), (uint8_t)(((uint32_t)(v) >> 24u) & 0xFFu)

/*
 * Test: event_pack_writes_header_and_one_record
 *
 * Scenario: A single key event is written correctly with proper header,
 *           record header, and payload in little-endian format.
 *
 * Arrange: Initialize 128-byte buffer with sentinel value.
 * Act:     Begin batch, append one key record, finish.
 * Assert:  Output matches expected byte-for-byte format.
 */
ZR_TEST_UNIT(event_pack_writes_header_and_one_record) {
  /* --- Arrange --- */
  uint8_t buf[128];
  memset(buf, 0xA5, sizeof(buf));

  zr_evpack_writer_t w;
  ZR_ASSERT_EQ_U32(zr_evpack_begin(&w, buf, sizeof(buf)), ZR_OK);

  const zr_ev_key_t payload = {
      .key = (uint32_t)ZR_KEY_ENTER,
      .mods = (uint32_t)ZR_MOD_SHIFT,
      .action = (uint32_t)ZR_KEY_ACTION_DOWN,
      .reserved0 = 0u,
  };

  /* --- Act --- */
  ZR_ASSERT_TRUE(zr_evpack_append_record(&w, ZR_EV_KEY, 123u, 0u, &payload, sizeof(payload)));
  const size_t n = zr_evpack_finish(&w);

  /* --- Assert: Matches expected binary format --- */
  const uint8_t expected[] = {
      /* zr_evbatch_header_t (6 u32) */
      ZR_U32LE(ZR_EV_MAGIC), ZR_U32LE(ZR_EVENT_BATCH_VERSION_V1), ZR_U32LE(56u), ZR_U32LE(1u),
      ZR_U32LE(0u), ZR_U32LE(0u),

      /* zr_ev_record_header_t (4 u32) */
      ZR_U32LE((uint32_t)ZR_EV_KEY), ZR_U32LE(32u), ZR_U32LE(123u), ZR_U32LE(0u),

      /* zr_ev_key_t (4 u32) */
      ZR_U32LE((uint32_t)ZR_KEY_ENTER), ZR_U32LE((uint32_t)ZR_MOD_SHIFT),
      ZR_U32LE((uint32_t)ZR_KEY_ACTION_DOWN), ZR_U32LE(0u),
  };

  ZR_ASSERT_EQ_U32((uint32_t)n, (uint32_t)sizeof(expected));
  ZR_ASSERT_MEMEQ(buf, expected, sizeof(expected));
}

/*
 * Test: event_pack_rejects_too_small_for_header
 *
 * Scenario: If the buffer is too small to fit the batch header, begin fails
 *           and the buffer is not modified.
 *
 * Arrange: Initialize 23-byte buffer (header requires 24 bytes).
 * Act:     Attempt to begin event batch.
 * Assert:  Returns ZR_ERR_LIMIT; buffer unchanged.
 */
ZR_TEST_UNIT(event_pack_rejects_too_small_for_header) {
  /* --- Arrange --- */
  uint8_t buf[23];
  uint8_t expected[23];
  memset(buf, 0xA5, sizeof(buf));
  memcpy(expected, buf, sizeof(expected));

  /* --- Act --- */
  zr_evpack_writer_t w;
  const zr_result_t rc = zr_evpack_begin(&w, buf, sizeof(buf));

  /* --- Assert: Fails, buffer untouched --- */
  ZR_ASSERT_EQ_U32(rc, ZR_ERR_LIMIT);
  ZR_ASSERT_MEMEQ(buf, expected, sizeof(buf));
}

/*
 * Test: event_pack_truncates_without_partial_record
 *
 * Scenario: When a record doesn't fit, append returns false, no partial
 *           record is written, and the truncated flag is set in the header.
 *
 * Arrange: Initialize 40-byte buffer (header=24, needs 56 for one record).
 * Act:     Begin batch, attempt to append key record (too big).
 * Assert:  Append fails; finish returns header-only size; truncated flag set;
 *          no bytes beyond header modified.
 */
ZR_TEST_UNIT(event_pack_truncates_without_partial_record) {
  /* --- Arrange --- */
  uint8_t buf[40];
  memset(buf, 0xA5, sizeof(buf));

  zr_evpack_writer_t w;
  ZR_ASSERT_EQ_U32(zr_evpack_begin(&w, buf, sizeof(buf)), ZR_OK);

  /* Record would require 24 (hdr) + 16 (rec hdr) + 16 (payload) = 56 bytes; doesn't fit. */
  zr_ev_key_t payload;
  memset(&payload, 0, sizeof(payload));

  /* --- Act --- */
  const bool appended = zr_evpack_append_record(&w, ZR_EV_KEY, 0u, 0u, &payload, sizeof(payload));
  const size_t n = zr_evpack_finish(&w);

  /* --- Assert: Append failed, header-only output with truncated flag --- */
  ZR_ASSERT_TRUE(!appended);

  const uint8_t expected_hdr[] = {
      ZR_U32LE(ZR_EV_MAGIC), ZR_U32LE(ZR_EVENT_BATCH_VERSION_V1), ZR_U32LE(24u), ZR_U32LE(0u),
      ZR_U32LE(ZR_EV_BATCH_TRUNCATED), ZR_U32LE(0u),
  };

  ZR_ASSERT_EQ_U32((uint32_t)n, 24u);
  ZR_ASSERT_MEMEQ(buf, expected_hdr, sizeof(expected_hdr));

  /* --- Assert: No bytes beyond header were touched --- */
  for (size_t i = n; i < sizeof(buf); i++) {
    if (buf[i] != 0xA5u) {
      ZR_ASSERT_TRUE(false);
      return;
    }
  }
}

/*
 * Test: event_pack_truncates_after_some_records_fit
 *
 * Scenario: When some records fit but not all, the successfully written
 *           records are preserved and the truncated flag is set.
 *
 * Arrange: Initialize 56-byte buffer.
 * Act:     Append 2 TEXT records; first fits, second doesn't.
 * Assert:  First record written; truncated flag set; event_count=1;
 *          padding bytes are zeroed.
 */
ZR_TEST_UNIT(event_pack_truncates_after_some_records_fit) {
  /* --- Arrange --- */
  uint8_t buf[56];
  memset(buf, 0xA5, sizeof(buf));

  zr_evpack_writer_t w;
  ZR_ASSERT_EQ_U32(zr_evpack_begin(&w, buf, sizeof(buf)), ZR_OK);

  const uint8_t b1 = 0xABu;
  const uint8_t b2 = 0xCDu;

  /* --- Act: First record fits, second doesn't --- */
  /* Record 1: type=TEXT with 1 byte payload => rec size = align4(16+1)=20. */
  ZR_ASSERT_TRUE(zr_evpack_append_record(&w, ZR_EV_TEXT, 1u, 0u, &b1, 1u));

  /* Record 2 won't fit: would need another 20 bytes (total 24+20+20=64 > 56). */
  ZR_ASSERT_TRUE(!zr_evpack_append_record(&w, ZR_EV_TEXT, 2u, 0u, &b2, 1u));
  const size_t n = zr_evpack_finish(&w);

  /* --- Assert: Header + one record, truncated flag set --- */
  ZR_ASSERT_EQ_U32((uint32_t)n, 44u);

  /* Validate flags and event_count in patched header. */
  const uint32_t total_size = (uint32_t)(buf[2 * 4 + 0] | (buf[2 * 4 + 1] << 8u) |
                                        (buf[2 * 4 + 2] << 16u) | (buf[2 * 4 + 3] << 24u));
  const uint32_t event_count = (uint32_t)(buf[3 * 4 + 0] | (buf[3 * 4 + 1] << 8u) |
                                         (buf[3 * 4 + 2] << 16u) | (buf[3 * 4 + 3] << 24u));
  const uint32_t flags = (uint32_t)(buf[4 * 4 + 0] | (buf[4 * 4 + 1] << 8u) |
                                   (buf[4 * 4 + 2] << 16u) | (buf[4 * 4 + 3] << 24u));

  ZR_ASSERT_EQ_U32(total_size, 44u);
  ZR_ASSERT_EQ_U32(event_count, 1u);
  ZR_ASSERT_EQ_U32(flags, ZR_EV_BATCH_TRUNCATED);

  /* --- Assert: Record 1 payload + padding zeroed --- */
  /* Payload is at offset 24+16 = 40. */
  ZR_ASSERT_EQ_U32(buf[40], 0xABu);
  ZR_ASSERT_EQ_U32(buf[41], 0u); /* padding */
  ZR_ASSERT_EQ_U32(buf[42], 0u);
  ZR_ASSERT_EQ_U32(buf[43], 0u);
}

