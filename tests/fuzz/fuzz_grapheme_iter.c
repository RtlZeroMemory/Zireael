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

/*
 * Fuzz one input: iterate all graphemes and verify progress invariants.
 *
 * Invariants verified:
 *   1. Every grapheme has size >= 1
 *   2. Grapheme offsets are contiguous (offset matches running total)
 *   3. Total bytes consumed equals input size (no skipped bytes)
 *   4. Iteration count never exceeds input size (no infinite loops)
 */
static void zr_fuzz_one(const uint8_t* data, size_t size) {
  zr_grapheme_iter_t it;
  zr_grapheme_iter_init(&it, data, size);

  size_t total = 0u;
  size_t count = 0u;
  zr_grapheme_t g;
  while (zr_grapheme_next(&it, &g)) {
    /* Each grapheme must have non-zero size and contiguous offset. */
    if (g.size == 0u || g.offset != total) {
      zr_fuzz_trap();
    }
    total += g.size;
    count++;
    /* Guard against infinite loops. */
    if (count > size) {
      zr_fuzz_trap();
    }
  }

  /* All input bytes must be consumed. */
  if (total != size) {
    zr_fuzz_trap();
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
