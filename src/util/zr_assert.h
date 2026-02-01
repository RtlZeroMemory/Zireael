/*
  src/util/zr_assert.h — Debug-only invariant assertions.

  Why: Provides inexpensive internal invariants that compile out in release
  builds. Never use these for validating untrusted inputs.
*/

#ifndef ZR_UTIL_ZR_ASSERT_H_INCLUDED
#define ZR_UTIL_ZR_ASSERT_H_INCLUDED

#if defined(NDEBUG)
#define ZR_ASSERT(expr) ((void)0)
#else
#include <stdlib.h>

static inline void zr__assert_fail(const char* file, int line, const char* expr) {
  (void)file;
  (void)line;
  (void)expr;
  abort();
}

#define ZR_ASSERT(expr)                          \
  do {                                           \
    if (!(expr)) {                               \
      zr__assert_fail(__FILE__, __LINE__, #expr); \
    }                                            \
  } while (0)
#endif

#endif /* ZR_UTIL_ZR_ASSERT_H_INCLUDED */

