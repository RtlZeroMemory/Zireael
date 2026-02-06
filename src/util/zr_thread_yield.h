/*
  src/util/zr_thread_yield.h — Portable cooperative spin-wait hint.

  Why: Core/util code cannot depend on OS headers, but tight spin loops should
  still provide a scheduler hint where C11 threads are available.
*/

#ifndef ZR_UTIL_ZR_THREAD_YIELD_H_INCLUDED
#define ZR_UTIL_ZR_THREAD_YIELD_H_INCLUDED

#include <stdatomic.h>

#if (!defined(__STDC_NO_THREADS__) || (__STDC_NO_THREADS__ == 0)) && defined(__has_include)
#if __has_include(<threads.h>)
#define ZR_UTIL_HAVE_C11_THREADS 1
#include <threads.h>
#endif
#endif

#ifndef ZR_UTIL_HAVE_C11_THREADS
#define ZR_UTIL_HAVE_C11_THREADS 0
#endif

static inline void zr_thread_yield(void) {
#if ZR_UTIL_HAVE_C11_THREADS
  thrd_yield();
#else
  /*
    C11 thread primitives are unavailable (e.g. AppleClang libc on macOS).
    Keep the loop defined and cooperative with a compiler+CPU fence fallback.
  */
  atomic_signal_fence(memory_order_seq_cst);
#endif
}

#endif /* ZR_UTIL_ZR_THREAD_YIELD_H_INCLUDED */
