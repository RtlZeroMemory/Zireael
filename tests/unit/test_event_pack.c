/*
  tests/unit/test_event_pack.c — Unit tests for packed event batch writer.
*/

#include "zr_test.h"

#include "core/zr_event_pack.h"

#include <string.h>

#define ZR_U32LE(v)                                                                    \
  (uint8_t)((uint32_t)(v)&0xFFu), (uint8_t)(((uint32_t)(v) >> 8u) & 0xFFu),             \
      (uint8_t)(((uint32_t)(v) >> 16u) & 0xFFu), (uint8_t)(((uint32_t)(v) >> 24u) & 0xFFu)

ZR_TEST_UNIT(event_pack_writes_header_and_one_record) {
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
  ZR_ASSERT_TRUE(zr_evpack_append_record(&w, ZR_EV_KEY, 123u, 0u, &payload, sizeof(payload)));
  const size_t n = zr_evpack_finish(&w);

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

ZR_TEST_UNIT(event_pack_rejects_too_small_for_header) {
  uint8_t buf[23];
  uint8_t expected[23];
  memset(buf, 0xA5, sizeof(buf));
  memcpy(expected, buf, sizeof(expected));

  zr_evpack_writer_t w;
  ZR_ASSERT_EQ_U32(zr_evpack_begin(&w, buf, sizeof(buf)), ZR_ERR_LIMIT);
  ZR_ASSERT_MEMEQ(buf, expected, sizeof(buf));
}

ZR_TEST_UNIT(event_pack_truncates_without_partial_record) {
  uint8_t buf[40];
  memset(buf, 0xA5, sizeof(buf));

  zr_evpack_writer_t w;
  ZR_ASSERT_EQ_U32(zr_evpack_begin(&w, buf, sizeof(buf)), ZR_OK);

  /* Would require 16 (hdr) + 16 (rec hdr) + 16 (payload) = 56 bytes total; doesn't fit. */
  zr_ev_key_t payload;
  memset(&payload, 0, sizeof(payload));

  ZR_ASSERT_TRUE(!zr_evpack_append_record(&w, ZR_EV_KEY, 0u, 0u, &payload, sizeof(payload)));
  const size_t n = zr_evpack_finish(&w);

  const uint8_t expected_hdr[] = {
      ZR_U32LE(ZR_EV_MAGIC), ZR_U32LE(ZR_EVENT_BATCH_VERSION_V1), ZR_U32LE(24u), ZR_U32LE(0u),
      ZR_U32LE(ZR_EV_BATCH_TRUNCATED), ZR_U32LE(0u),
  };

  ZR_ASSERT_EQ_U32((uint32_t)n, 24u);
  ZR_ASSERT_MEMEQ(buf, expected_hdr, sizeof(expected_hdr));

  /* Confirm no bytes beyond header were touched. */
  for (size_t i = n; i < sizeof(buf); i++) {
    if (buf[i] != 0xA5u) {
      ZR_ASSERT_TRUE(false);
      return;
    }
  }
}

ZR_TEST_UNIT(event_pack_truncates_after_some_records_fit) {
  uint8_t buf[56];
  memset(buf, 0xA5, sizeof(buf));

  zr_evpack_writer_t w;
  ZR_ASSERT_EQ_U32(zr_evpack_begin(&w, buf, sizeof(buf)), ZR_OK);

  const uint8_t b1 = 0xABu;
  const uint8_t b2 = 0xCDu;

  /* Record 1: type=TEXT with 1 byte payload => rec size = align4(16+1)=20. */
  ZR_ASSERT_TRUE(zr_evpack_append_record(&w, ZR_EV_TEXT, 1u, 0u, &b1, 1u));

  /* Record 2 won't fit: would need another 20 bytes (total 24+20+20=64 > 56). */
  ZR_ASSERT_TRUE(!zr_evpack_append_record(&w, ZR_EV_TEXT, 2u, 0u, &b2, 1u));
  const size_t n = zr_evpack_finish(&w);

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

  /* Record 1 padding must be zero. Payload is at offset 24+16 = 40. */
  ZR_ASSERT_EQ_U32(buf[40], 0xABu);
  ZR_ASSERT_EQ_U32(buf[41], 0u);
  ZR_ASSERT_EQ_U32(buf[42], 0u);
  ZR_ASSERT_EQ_U32(buf[43], 0u);
}

