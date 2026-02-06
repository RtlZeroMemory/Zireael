/*
  src/platform/posix/zr_plat_posix_test.h — POSIX backend integration test hooks.

  Why: Exposes deterministic, POSIX-only test controls for integration tests
  without widening the public platform ABI in include/platform/.
*/

#ifndef ZR_PLATFORM_POSIX_ZR_PLAT_POSIX_TEST_H_INCLUDED
#define ZR_PLATFORM_POSIX_ZR_PLAT_POSIX_TEST_H_INCLUDED

#include <stdint.h>

/*
  Enable/disable forced SIGWINCH overflow-marker behavior.

  Why: Integration tests must exercise overflow wake preservation without
  depending on host-specific pipe capacity and scheduling.
*/
void zr_posix_test_force_sigwinch_overflow(uint8_t enabled);

#endif /* ZR_PLATFORM_POSIX_ZR_PLAT_POSIX_TEST_H_INCLUDED */
