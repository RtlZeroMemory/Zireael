/*
  tests/unit/test_string_builder.c — Unit tests for util/zr_string_builder.h.
*/

#include "zr_test.h"

#include "util/zr_string_builder.h"

#include <stdint.h>

ZR_TEST_UNIT(sb_no_partial_write_on_overflow) {
  uint8_t buf[4] = {0u, 0u, 0u, 0u};
  zr_sb_t sb;
  zr_sb_init(&sb, buf, sizeof(buf));

  ZR_ASSERT_TRUE(zr_sb_write_u32le(&sb, 0x11223344u));
  ZR_ASSERT_EQ_U32((uint32_t)zr_sb_len(&sb), 4u);
  ZR_ASSERT_TRUE(!zr_sb_truncated(&sb));

  /* No space left: must not mutate len or bytes; must set truncated. */
  ZR_ASSERT_TRUE(!zr_sb_write_u8(&sb, 0x55u));
  ZR_ASSERT_EQ_U32((uint32_t)zr_sb_len(&sb), 4u);
  ZR_ASSERT_TRUE(zr_sb_truncated(&sb));

  const uint8_t expect[4] = {0x44u, 0x33u, 0x22u, 0x11u};
  ZR_ASSERT_MEMEQ(buf, expect, sizeof(expect));
}

ZR_TEST_UNIT(sb_write_bytes_overflow_sets_truncated) {
  uint8_t buf[3] = {0u, 0u, 0u};
  zr_sb_t sb;
  zr_sb_init(&sb, buf, sizeof(buf));

  const uint8_t bytes[4] = {1u, 2u, 3u, 4u};
  ZR_ASSERT_TRUE(!zr_sb_write_bytes(&sb, bytes, sizeof(bytes)));
  ZR_ASSERT_TRUE(zr_sb_truncated(&sb));
  ZR_ASSERT_EQ_U32((uint32_t)zr_sb_len(&sb), 0u);
}

