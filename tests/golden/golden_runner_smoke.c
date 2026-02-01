/*
  tests/golden/golden_runner_smoke.c — Golden runner smoke tests.

  Why: Validates that the golden harness detects exact matches, mismatches, and
  missing fixtures with deterministic, actionable diagnostics.
*/

#include "golden/zr_golden.h"
#include "zr_test.h"

ZR_TEST_GOLDEN(pass) {
  const uint8_t actual[] = {0x00u, 0x01u, 0xFEu, 0xFFu};
  const int rc = zr_golden_compare_fixture("zr_bytes_smoke_00", actual, sizeof(actual));
  ZR_ASSERT_EQ_U32((uint32_t)rc, 0u);
}

ZR_TEST_GOLDEN(fail_mismatch) {
  const uint8_t actual[] = {0x00u, 0x01u, 0xFDu, 0xFFu};
  const int rc = zr_golden_compare_fixture("zr_bytes_smoke_00", actual, sizeof(actual));
  ZR_ASSERT_TRUE(rc != 0);
}

ZR_TEST_GOLDEN(fail_missing_fixture) {
  const uint8_t actual[] = {0x00u};
  const int rc = zr_golden_compare_fixture("zr_fixture_does_not_exist", actual, sizeof(actual));
  ZR_ASSERT_TRUE(rc != 0);
}
