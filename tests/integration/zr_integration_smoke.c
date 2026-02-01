/*
  tests/integration/zr_integration_smoke.c — Integration test scaffold.

  Why: Provides a deterministic "skip with explicit reason" test binary so CTest
  never passes-by-absence when platform backends are not implemented yet.
*/

#include <stdio.h>

int main(void) {
  /*
    Autoconf-style skip code: CTest can be configured to treat this as SKIPPED.
    Keep the reason stable and explicit.
  */
  fprintf(stdout, "SKIP: integration backends not implemented yet (EPIC-007/#29, EPIC-008/#30)\n");
  return 77;
}

