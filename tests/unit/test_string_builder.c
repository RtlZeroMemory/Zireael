/*
  tests/unit/test_string_builder.c — Unit tests for util/zr_string_builder.h.

  Why: Validates string builder contracts including "no partial write" on
  overflow, truncation flag behavior, and defensive checks against corrupted
  state.

  Scenarios tested:
    - Successful write followed by overflow sets truncated flag
    - Overflow does not mutate buffer contents or length
    - write_bytes overflow sets truncated without partial write
    - Corrupted len > cap is handled safely (no underflow/OOB write)
*/

#include "zr_test.h"

#include "util/zr_string_builder.h"

#include <stdint.h>

/*
 * Test: sb_no_partial_write_on_overflow
 *
 * Scenario: When remaining space is insufficient, write operations fail
 *           without modifying the buffer and set the truncated flag.
 *
 * Arrange: Initialize 4-byte string builder.
 * Act:     Write u32 (fills buffer), attempt u8 write (no space).
 * Assert:  u32 write succeeds; u8 write fails, length unchanged, truncated set.
 */
ZR_TEST_UNIT(sb_no_partial_write_on_overflow) {
  /* --- Arrange --- */
  uint8_t buf[4] = {0u, 0u, 0u, 0u};
  zr_sb_t sb;
  zr_sb_init(&sb, buf, sizeof(buf));

  /* --- Act: Fill buffer with u32 --- */
  ZR_ASSERT_TRUE(zr_sb_write_u32le(&sb, 0x11223344u));
  ZR_ASSERT_EQ_U32((uint32_t)zr_sb_len(&sb), 4u);
  ZR_ASSERT_TRUE(!zr_sb_truncated(&sb));

  /* --- Act: Attempt write when full --- */
  ZR_ASSERT_TRUE(!zr_sb_write_u8(&sb, 0x55u));

  /* --- Assert: No mutation, truncated flag set --- */
  ZR_ASSERT_EQ_U32((uint32_t)zr_sb_len(&sb), 4u);
  ZR_ASSERT_TRUE(zr_sb_truncated(&sb));

  const uint8_t expect[4] = {0x44u, 0x33u, 0x22u, 0x11u};
  ZR_ASSERT_MEMEQ(buf, expect, sizeof(expect));
}

/*
 * Test: sb_write_bytes_overflow_sets_truncated
 *
 * Scenario: write_bytes rejects writes that exceed capacity without writing
 *           any partial data.
 *
 * Arrange: Initialize 3-byte string builder.
 * Act:     Attempt to write 4-byte array.
 * Assert:  Write fails; length remains 0; truncated flag set.
 */
ZR_TEST_UNIT(sb_write_bytes_overflow_sets_truncated) {
  /* --- Arrange --- */
  uint8_t buf[3] = {0u, 0u, 0u};
  zr_sb_t sb;
  zr_sb_init(&sb, buf, sizeof(buf));

  /* --- Act: Attempt oversized write --- */
  const uint8_t bytes[4] = {1u, 2u, 3u, 4u};
  ZR_ASSERT_TRUE(!zr_sb_write_bytes(&sb, bytes, sizeof(bytes)));

  /* --- Assert: No partial write, truncated set --- */
  ZR_ASSERT_TRUE(zr_sb_truncated(&sb));
  ZR_ASSERT_EQ_U32((uint32_t)zr_sb_len(&sb), 0u);
}

/*
 * Test: sb_guard_len_over_cap
 *
 * Scenario: If internal state is corrupted (len > cap), writes are rejected
 *           without causing underflow or out-of-bounds access.
 *
 * Arrange: Initialize 4-byte string builder, manually corrupt len to exceed cap.
 * Act:     Attempt u8 write.
 * Assert:  Write fails; truncated flag set; buffer unchanged.
 */
ZR_TEST_UNIT(sb_guard_len_over_cap) {
  /* --- Arrange --- */
  uint8_t buf[4] = {0u, 0u, 0u, 0u};
  zr_sb_t sb;
  zr_sb_init(&sb, buf, sizeof(buf));

  /* Simulate corrupted state: len exceeds capacity. */
  sb.len = 5u;

  /* --- Act: Attempt write on corrupted state --- */
  ZR_ASSERT_TRUE(!zr_sb_write_u8(&sb, 0x11u));

  /* --- Assert: Fails safely, no OOB write --- */
  ZR_ASSERT_TRUE(zr_sb_truncated(&sb));

  const uint8_t expect[4] = {0u, 0u, 0u, 0u};
  ZR_ASSERT_MEMEQ(buf, expect, sizeof(expect));
}
