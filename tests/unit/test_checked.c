/*
  tests/unit/test_checked.c — Unit tests for util/zr_checked.h.
*/

#include "zr_test.h"

#include "util/zr_checked.h"

#include <stdint.h>

ZR_TEST_UNIT(checked_add_overflow_no_mutate) {
  size_t out = 123u;
  ZR_ASSERT_TRUE(!zr_checked_add_size(SIZE_MAX, 1u, &out));
  ZR_ASSERT_EQ_U32((uint32_t)out, 123u);
}

ZR_TEST_UNIT(checked_mul_overflow_no_mutate) {
  size_t out = 7u;
  const size_t half = (SIZE_MAX / 2u) + 1u;
  ZR_ASSERT_TRUE(!zr_checked_mul_size(half, 2u, &out));
  ZR_ASSERT_EQ_U32((uint32_t)out, 7u);
}

ZR_TEST_UNIT(checked_align_up) {
  size_t out = 0u;
  ZR_ASSERT_TRUE(zr_checked_align_up_size(0u, 8u, &out));
  ZR_ASSERT_EQ_U32(out, 0u);
  ZR_ASSERT_TRUE(zr_checked_align_up_size(1u, 8u, &out));
  ZR_ASSERT_EQ_U32(out, 8u);
  ZR_ASSERT_TRUE(zr_checked_align_up_size(9u, 8u, &out));
  ZR_ASSERT_EQ_U32(out, 16u);
  ZR_ASSERT_TRUE(!zr_checked_align_up_size(1u, 0u, &out));
  ZR_ASSERT_TRUE(!zr_checked_align_up_size(1u, 3u, &out)); /* not pow2 */
}

