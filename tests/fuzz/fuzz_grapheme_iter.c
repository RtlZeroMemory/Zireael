/*
  tests/fuzz/fuzz_grapheme_iter.c — Grapheme iterator fuzz target (smoke-mode).

  Why: Ensures grapheme iteration never crashes/hangs and always advances using
  the project's locked UTF-8 replacement behavior.
*/

#include "unicode/zr_grapheme.h"
#include "zr_fuzz_config.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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
  enum {
    ZR_FUZZ_DEFAULT_ITERS = 1000,
    ZR_FUZZ_DEFAULT_MAX_SIZE = 512,
    ZR_FUZZ_MAX_ITERS = 2000000,
    ZR_FUZZ_MAX_INPUT_SIZE = 65536,
  };

  const int iters = zr_fuzz_env_int("ZR_FUZZ_ITERS", ZR_FUZZ_DEFAULT_ITERS, 1, ZR_FUZZ_MAX_ITERS);
  const int max_size_i = zr_fuzz_env_int("ZR_FUZZ_MAX_SIZE", ZR_FUZZ_DEFAULT_MAX_SIZE, 1, ZR_FUZZ_MAX_INPUT_SIZE);
  const size_t max_size = (size_t)max_size_i;

  uint32_t seed = 0xC0FFEEu;
  uint8_t* buf = (uint8_t*)malloc(max_size);
  if (!buf) {
    return 2;
  }

  for (int i = 0; i < iters; i++) {
    const size_t sz = (size_t)(zr_xorshift32(&seed) % (uint32_t)max_size_i);
    for (size_t j = 0; j < sz; j++) {
      buf[j] = (uint8_t)(zr_xorshift32(&seed) & 0xFFu);
    }
    zr_fuzz_one(buf, sz);
  }

  free(buf);
  return 0;
}
