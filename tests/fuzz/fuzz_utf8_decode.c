/*
  tests/fuzz/fuzz_utf8_decode.c — UTF-8 decoder fuzz target (smoke-mode).

  Why: Ensures the decoder never crashes/hangs and always makes progress on
  arbitrary bytes without reading out of bounds.
*/

#include "unicode/zr_utf8.h"

#include <stddef.h>
#include <stdint.h>

static void zr_fuzz_trap(void) {
  volatile int* p = (volatile int*)0;
  *p = 1;
}

static uint32_t zr_xorshift32(uint32_t* state) {
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

static void zr_fuzz_one(const uint8_t* data, size_t size) {
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
}

int main(void) {
  enum { kIters = 1000, kMaxSize = 512 };
  uint32_t seed = 0xC0FFEEu;
  uint8_t buf[kMaxSize];

  for (int i = 0; i < kIters; i++) {
    const size_t sz = (size_t)(zr_xorshift32(&seed) % (uint32_t)kMaxSize);
    for (size_t j = 0; j < sz; j++) {
      buf[j] = (uint8_t)(zr_xorshift32(&seed) & 0xFFu);
    }
    zr_fuzz_one(buf, sz);
  }

  return 0;
}
