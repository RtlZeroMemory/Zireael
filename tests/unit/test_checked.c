/*
  tests/unit/test_checked.c — Unit tests for util/zr_checked.h.

  Why: Validates checked arithmetic helpers that detect overflow without
  undefined behavior, ensuring the "no partial effects" contract when
  overflow is detected.

  Scenarios tested:
    - Addition overflow: SIZE_MAX + 1 detected, output unchanged
    - Multiplication overflow: half of SIZE_MAX * 2 detected, output unchanged
    - Alignment: align_up rounds correctly for power-of-two alignments
    - Alignment rejects invalid inputs (zero, non-power-of-two)
*/

#include "zr_test.h"

#include "util/zr_checked.h"

#include <stdint.h>

/*
 * Test: checked_add_overflow_no_mutate
 *
 * Scenario: Checked addition detects overflow and does not modify the
 *           output parameter when overflow occurs.
 *
 * Arrange: Set output to known sentinel value.
 * Act:     Attempt SIZE_MAX + 1.
 * Assert:  Returns false (overflow); output unchanged.
 */
ZR_TEST_UNIT(checked_add_overflow_no_mutate) {
  /* --- Arrange --- */
  size_t out = 123u;

  /* --- Act & Assert: Overflow detected, output unchanged --- */
  ZR_ASSERT_TRUE(!zr_checked_add_size(SIZE_MAX, 1u, &out));
  ZR_ASSERT_EQ_U32((uint32_t)out, 123u);
}

/*
 * Test: checked_mul_overflow_no_mutate
 *
 * Scenario: Checked multiplication detects overflow and does not modify
 *           the output parameter when overflow occurs.
 *
 * Arrange: Set output to known sentinel; compute half of SIZE_MAX + 1.
 * Act:     Attempt (SIZE_MAX/2 + 1) * 2 (overflows).
 * Assert:  Returns false (overflow); output unchanged.
 */
ZR_TEST_UNIT(checked_mul_overflow_no_mutate) {
  /* --- Arrange --- */
  size_t out = 7u;
  const size_t half = (SIZE_MAX / 2u) + 1u;

  /* --- Act & Assert: Overflow detected, output unchanged --- */
  ZR_ASSERT_TRUE(!zr_checked_mul_size(half, 2u, &out));
  ZR_ASSERT_EQ_U32((uint32_t)out, 7u);
}

/*
 * Test: checked_align_up
 *
 * Scenario: Checked alignment rounds up to the next multiple of a power-of-two
 *           alignment and rejects invalid alignment values.
 *
 * Arrange: Initialize output variable.
 * Act:     Align 0, 1, and 9 to 8-byte boundary; attempt align with 0 and 3.
 * Assert:  Valid alignments: 0→0, 1→8, 9→16. Invalid alignments return false.
 */
ZR_TEST_UNIT(checked_align_up) {
  /* --- Arrange --- */
  size_t out = 0u;

  /* --- Act & Assert: Valid alignments --- */
  ZR_ASSERT_TRUE(zr_checked_align_up_size(0u, 8u, &out));
  ZR_ASSERT_EQ_U32(out, 0u);

  ZR_ASSERT_TRUE(zr_checked_align_up_size(1u, 8u, &out));
  ZR_ASSERT_EQ_U32(out, 8u);

  ZR_ASSERT_TRUE(zr_checked_align_up_size(9u, 8u, &out));
  ZR_ASSERT_EQ_U32(out, 16u);

  /* --- Act & Assert: Invalid alignment (zero) --- */
  ZR_ASSERT_TRUE(!zr_checked_align_up_size(1u, 0u, &out));

  /* --- Act & Assert: Invalid alignment (not power-of-two) --- */
  ZR_ASSERT_TRUE(!zr_checked_align_up_size(1u, 3u, &out));
}

