/*
  tests/unit/test_bytes.c — Unit tests for util/zr_bytes.h.

  Why: Validates little-endian load/store helpers for unaligned access
  (portable byte manipulation without type-punning) and the byte reader's
  "no partial read" contract on failure.

  Scenarios tested:
    - Unaligned LE load/store for u16, u32, u64
    - Byte reader never advances offset on failed read or skip
    - Byte reader with NULL bytes always fails reads and skips
*/

#include "zr_test.h"

#include "util/zr_bytes.h"

#include <stdint.h>

/*
 * Test: bytes_unaligned_le_load_store
 *
 * Scenario: Little-endian load/store helpers work correctly at unaligned
 *           offsets (simulating packed binary format access).
 *
 * Arrange: Zero-initialized 16-byte buffer.
 * Act:     Store u32 at offset 1, u16 at offset 3, u64 at offset 5; load each back.
 * Assert:  Loaded values match stored values.
 */
ZR_TEST_UNIT(bytes_unaligned_le_load_store) {
  /* --- Arrange --- */
  uint8_t buf[16] = {0};

  /* --- Act & Assert: u32 at unaligned offset --- */
  zr_store_u32le(buf + 1, 0x11223344u);
  ZR_ASSERT_EQ_U32(zr_load_u32le(buf + 1), 0x11223344u);

  /* --- Act & Assert: u16 at unaligned offset --- */
  zr_store_u16le(buf + 3, (uint16_t)0xABCDu);
  ZR_ASSERT_EQ_U32(zr_load_u16le(buf + 3), 0xABCDu);

  /* --- Act & Assert: u64 at unaligned offset --- */
  zr_store_u64le(buf + 5, 0x0102030405060708ull);
  ZR_ASSERT_TRUE(zr_load_u64le(buf + 5) == 0x0102030405060708ull);
}

/*
 * Test: bytes_reader_never_advances_on_failure
 *
 * Scenario: When a read or skip would exceed bounds, the byte reader
 *           returns false and does not advance the offset ("no partial read").
 *
 * Arrange: Initialize reader over 4-byte buffer.
 * Act:     Read u32 (consumes all), attempt u16 read, attempt skip.
 * Assert:  u32 read succeeds and advances offset; u16 read and skip fail
 *          without changing offset.
 */
ZR_TEST_UNIT(bytes_reader_never_advances_on_failure) {
  /* --- Arrange --- */
  const uint8_t buf[4] = {0x01u, 0x02u, 0x03u, 0x04u};
  zr_byte_reader_t r;
  zr_byte_reader_init(&r, buf, sizeof(buf));

  /* --- Act: Successful u32 read --- */
  uint32_t v32 = 0u;
  ZR_ASSERT_TRUE(zr_byte_reader_read_u32le(&r, &v32));
  ZR_ASSERT_EQ_U32(v32, 0x04030201u);
  ZR_ASSERT_EQ_U32((uint32_t)r.off, 4u);

  /* --- Act & Assert: Failed read does not advance offset --- */
  uint16_t v16 = 0u;
  ZR_ASSERT_TRUE(!zr_byte_reader_read_u16le(&r, &v16));
  ZR_ASSERT_EQ_U32((uint32_t)r.off, 4u);

  /* --- Act & Assert: Failed skip does not advance offset --- */
  ZR_ASSERT_TRUE(!zr_byte_reader_skip(&r, 1u));
  ZR_ASSERT_EQ_U32((uint32_t)r.off, 4u);
}

/*
 * Test: bytes_reader_null_bytes_never_reads_or_skips
 *
 * Scenario: A byte reader initialized with NULL bytes pointer rejects
 *           all read and skip operations (defensive against NULL deref).
 *
 * Arrange: Initialize reader with NULL bytes and non-zero length.
 * Act:     Attempt u8 read, attempt skip.
 * Assert:  Both fail; offset remains 0.
 */
ZR_TEST_UNIT(bytes_reader_null_bytes_never_reads_or_skips) {
  /* --- Arrange --- */
  zr_byte_reader_t r;
  zr_byte_reader_init(&r, NULL, 4u);

  /* --- Act & Assert: Read fails --- */
  uint8_t v8 = 0u;
  ZR_ASSERT_TRUE(!zr_byte_reader_read_u8(&r, &v8));
  ZR_ASSERT_EQ_U32((uint32_t)r.off, 0u);

  /* --- Act & Assert: Skip fails --- */
  ZR_ASSERT_TRUE(!zr_byte_reader_skip(&r, 1u));
  ZR_ASSERT_EQ_U32((uint32_t)r.off, 0u);
}
