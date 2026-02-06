/*
  tests/fuzz/libfuzzer_utf8_decode.c — Coverage-guided UTF-8 decode progress harness.

  Why: Verifies decoder progress and replacement behavior under libFuzzer-guided
  byte streams.
*/

#include "unicode/zr_utf8.h"

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
  size_t off = 0u;
  while (off < size) {
    zr_utf8_decode_result_t r = zr_utf8_decode_one(data + off, size - off);
    if (r.size == 0u || (size_t)r.size > (size - off)) {
      zr_fuzz_trap();
    }
    if (r.valid == 0u) {
      if (r.scalar != 0xFFFDu || r.size != 1u) {
        zr_fuzz_trap();
      }
    }
    off += (size_t)r.size;
  }
  return 0;
}
