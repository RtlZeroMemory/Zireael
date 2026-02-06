/*
  tests/fuzz/libfuzzer_grapheme_iter.c — Coverage-guided grapheme iterator harness.

  Why: Ensures grapheme iteration always progresses and fully consumes the input
  stream under guided corpus mutation.
*/

#include "unicode/zr_grapheme.h"

#include <stddef.h>
#include <stdint.h>

static void zr_fuzz_trap(void) {
#if defined(_MSC_VER)
  __debugbreak();
#elif defined(__GNUC__) || defined(__clang__)
  __builtin_trap();
#else
  volatile int* p = (volatile int*)0;
  *p = 1;
#endif
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  zr_grapheme_iter_t it;
  zr_grapheme_iter_init(&it, data, size);

  size_t total = 0u;
  size_t count = 0u;
  zr_grapheme_t g;
  while (zr_grapheme_next(&it, &g)) {
    if (g.size == 0u || g.offset != total) {
      zr_fuzz_trap();
    }
    total += g.size;
    count++;
    if (count > size + 1u) {
      zr_fuzz_trap();
    }
  }
  if (total != size) {
    zr_fuzz_trap();
  }
  return 0;
}
