/*
  tests/unit/unit_smoke.c — Unit test scaffold.

  Why: Ensures CTest wiring is functional before real unit tests are implemented.
*/

#include "zr_test.h"

ZR_TEST_UNIT(smoke) {
  ZR_ASSERT_TRUE(1);
  ZR_ASSERT_EQ_U32(123u, 123u);

  const uint8_t a[4] = {0x00u, 0x01u, 0xFEu, 0xFFu};
  const uint8_t b[4] = {0x00u, 0x01u, 0xFEu, 0xFFu};
  ZR_ASSERT_MEMEQ(a, b, sizeof(a));
}
