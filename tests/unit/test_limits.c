/*
  tests/unit/test_limits.c — Unit tests for util/zr_caps.h.

  Why: Validates that the default limits structure has sensible non-zero
  values and that validation rejects invalid configurations.

  Scenarios tested:
    - Default limits have all non-zero values and pass validation
    - Zero values for required fields cause validation failure
    - Invalid relationships (initial > max) cause validation failure
*/

#include "zr_test.h"

#include "util/zr_caps.h"

/*
 * Test: limits_default_and_validate
 *
 * Scenario: The default limits structure contains sensible non-zero values
 *           for all capacity fields and passes validation.
 *
 * Arrange: Obtain default limits.
 * Act:     Check all fields and call validate.
 * Assert:  All capacity fields are non-zero; validate returns ZR_OK.
 */
ZR_TEST_UNIT(limits_default_and_validate) {
  /* --- Arrange --- */
  zr_limits_t l = zr_limits_default();

  /* --- Assert: All capacity fields are non-zero --- */
  ZR_ASSERT_TRUE(l.arena_max_total_bytes != 0u);
  ZR_ASSERT_TRUE(l.arena_initial_bytes != 0u);
  ZR_ASSERT_TRUE(l.out_max_bytes_per_frame != 0u);
  ZR_ASSERT_TRUE(l.dl_max_total_bytes != 0u);
  ZR_ASSERT_TRUE(l.dl_max_cmds != 0u);
  ZR_ASSERT_TRUE(l.dl_max_strings != 0u);
  ZR_ASSERT_TRUE(l.dl_max_blobs != 0u);
  ZR_ASSERT_TRUE(l.dl_max_clip_depth != 0u);
  ZR_ASSERT_TRUE(l.dl_max_text_run_segments != 0u);

  /* --- Assert: Validation passes --- */
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_OK);
}

/*
 * Test: limits_validate_rejects_zero_or_invalid
 *
 * Scenario: Validation rejects limits structures with zero values for
 *           required fields or invalid relationships between fields.
 *
 * Arrange: Start with default limits, modify one field at a time.
 * Act:     Call validate with each invalid configuration.
 * Assert:  Each returns ZR_ERR_INVALID_ARGUMENT.
 */
ZR_TEST_UNIT(limits_validate_rejects_zero_or_invalid) {
  zr_limits_t l;

  /* --- Zero arena_max_total_bytes --- */
  l = zr_limits_default();
  l.arena_max_total_bytes = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  /* --- Zero arena_initial_bytes --- */
  l = zr_limits_default();
  l.arena_initial_bytes = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  /* --- Initial exceeds max (invalid relationship) --- */
  l = zr_limits_default();
  l.arena_initial_bytes = l.arena_max_total_bytes + 1u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  /* --- Zero dl_max_total_bytes --- */
  l = zr_limits_default();
  l.dl_max_total_bytes = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  /* --- Zero out_max_bytes_per_frame --- */
  l = zr_limits_default();
  l.out_max_bytes_per_frame = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);
}
