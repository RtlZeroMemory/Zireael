/*
  tests/unit/test_bytes.c — Unit tests for util/zr_bytes.h.
*/

#include "zr_test.h"

#include "util/zr_bytes.h"

#include <stdint.h>

ZR_TEST_UNIT(bytes_unaligned_le_load_store) {
  uint8_t buf[16] = {0};

  zr_store_u32le(buf + 1, 0x11223344u);
  ZR_ASSERT_EQ_U32(zr_load_u32le(buf + 1), 0x11223344u);

  zr_store_u16le(buf + 3, (uint16_t)0xABCDu);
  ZR_ASSERT_EQ_U32(zr_load_u16le(buf + 3), 0xABCDu);

  zr_store_u64le(buf + 5, 0x0102030405060708ull);
  ZR_ASSERT_TRUE(zr_load_u64le(buf + 5) == 0x0102030405060708ull);
}

ZR_TEST_UNIT(bytes_reader_never_advances_on_failure) {
  const uint8_t buf[4] = {0x01u, 0x02u, 0x03u, 0x04u};
  zr_byte_reader_t r;
  zr_byte_reader_init(&r, buf, sizeof(buf));

  uint32_t v32 = 0u;
  ZR_ASSERT_TRUE(zr_byte_reader_read_u32le(&r, &v32));
  ZR_ASSERT_EQ_U32(v32, 0x04030201u);
  ZR_ASSERT_EQ_U32((uint32_t)r.off, 4u);

  /* Failed read: off must not change. */
  uint16_t v16 = 0u;
  ZR_ASSERT_TRUE(!zr_byte_reader_read_u16le(&r, &v16));
  ZR_ASSERT_EQ_U32((uint32_t)r.off, 4u);

  /* Failed skip: off must not change. */
  ZR_ASSERT_TRUE(!zr_byte_reader_skip(&r, 1u));
  ZR_ASSERT_EQ_U32((uint32_t)r.off, 4u);
}

