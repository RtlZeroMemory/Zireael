/*
  tests/golden/golden_runner_smoke.c — Golden runner smoke tests.

  Why: Validates that the golden harness detects exact matches, mismatches, and
  missing fixtures with deterministic, actionable diagnostics.
*/

#include "golden/zr_golden.h"
#include "zr_test.h"

/*
 * Test: pass
 *
 * Scenario: When actual bytes exactly match expected.bin, compare returns 0.
 *
 * Arrange: Bytes matching zr_bytes_smoke_00/expected.bin.
 * Act:     Compare with fixture.
 * Assert:  Returns 0 (match).
 */
ZR_TEST_GOLDEN(pass) {
  /* --- Arrange --- */
  const uint8_t actual[] = {0x00u, 0x01u, 0xFEu, 0xFFu};

  /* --- Act & Assert --- */
  const int rc = zr_golden_compare_fixture("zr_bytes_smoke_00", actual, sizeof(actual));
  ZR_ASSERT_EQ_U32((uint32_t)rc, 0u);
}

/*
 * Test: fail_mismatch
 *
 * Scenario: When actual bytes differ from expected.bin, compare returns non-zero.
 *
 * Arrange: Bytes differing at offset 2 (0xFD instead of 0xFE).
 * Act:     Compare with fixture.
 * Assert:  Returns non-zero (mismatch).
 */
ZR_TEST_GOLDEN(fail_mismatch) {
  /* --- Arrange --- */
  const uint8_t actual[] = {0x00u, 0x01u, 0xFDu, 0xFFu}; /* 0xFD != 0xFE */

  /* --- Act & Assert --- */
  const int rc = zr_golden_compare_fixture("zr_bytes_smoke_00", actual, sizeof(actual));
  ZR_ASSERT_TRUE(rc != 0);
}

/*
 * Test: fail_missing_fixture
 *
 * Scenario: When the fixture directory doesn't exist, compare returns non-zero.
 *
 * Arrange: Reference non-existent fixture ID.
 * Act:     Compare with missing fixture.
 * Assert:  Returns non-zero (missing fixture).
 */
ZR_TEST_GOLDEN(fail_missing_fixture) {
  /* --- Arrange --- */
  const uint8_t actual[] = {0x00u};

  /* --- Act & Assert --- */
  const int rc = zr_golden_compare_fixture("zr_fixture_does_not_exist", actual, sizeof(actual));
  ZR_ASSERT_TRUE(rc != 0);
}
