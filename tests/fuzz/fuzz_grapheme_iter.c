/*
  tests/fuzz/fuzz_grapheme_iter.c — Grapheme iterator fuzz target (smoke-mode).

  Why: Ensures grapheme iteration never crashes/hangs and always advances using
  the project's locked UTF-8 replacement behavior.
*/

#include "unicode/zr_grapheme.h"

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
    if (count > size) {
      zr_fuzz_trap();
    }
  }

  if (total != size) {
    zr_fuzz_trap();
  }
}

int main(void) {
  enum { kIters = 200, kMaxSize = 256 };
  uint32_t seed = 0xC0FFEEu;
  uint8_t  buf[kMaxSize];

  for (int i = 0; i < kIters; i++) {
    const size_t sz = (size_t)(zr_xorshift32(&seed) % (uint32_t)kMaxSize);
    for (size_t j = 0; j < sz; j++) {
      buf[j] = (uint8_t)(zr_xorshift32(&seed) & 0xFFu);
    }
    zr_fuzz_one(buf, sz);
  }

  return 0;
}

