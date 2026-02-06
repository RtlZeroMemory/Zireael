/*
  tests/fuzz/fuzz_drawlist_parser.c — Drawlist validator fuzz target (smoke-mode).

  Why: Validates that the drawlist parser never crashes, hangs, or exhibits
  non-deterministic behavior when fed arbitrary bytes. Uses a deterministic
  PRNG to generate test inputs without requiring libFuzzer.

  Invariants verified:
    - Parser never crashes on malformed input
    - Same input always produces same return code (determinism)
    - Same input always produces identical zr_dl_view_t output
*/

#include "core/zr_drawlist.h"

#include <stdint.h>
#include <string.h>

/* Trigger a trap/crash for test failure detection by fuzzers/sanitizers. */
static void zr_fuzz_trap(void) {
#if defined(_MSC_VER)
  __debugbreak();
#elif defined(__GNUC__) || defined(__clang__)
  __builtin_trap();
#else
  /* Best-effort fallback. */
  volatile int* p = (volatile int*)0;
  *p = 0;
#endif
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
 * Fuzz one input: validate drawlist bytes twice and verify determinism.
 *
 * Checks:
 *   1. Validator doesn't crash on arbitrary bytes
 *   2. Same input produces same result code (determinism)
 *   3. Same input produces identical parsed header (if valid)
 */
static void zr_fuzz_one(const uint8_t* data, size_t size) {
  /* Configure limits to allow input size but cap internal structures. */
  zr_limits_t lim = zr_limits_default();
  lim.dl_max_total_bytes = (uint32_t)size;
  lim.dl_max_cmds = 64u;
  lim.dl_max_strings = 64u;
  lim.dl_max_blobs = 64u;
  lim.dl_max_clip_depth = 16u;
  lim.dl_max_text_run_segments = 64u;

  /* Validate same input twice. */
  zr_dl_view_t v1;
  zr_dl_view_t v2;
  const zr_result_t r1 = zr_dl_validate(data, size, &lim, &v1);
  const zr_result_t r2 = zr_dl_validate(data, size, &lim, &v2);

  /* Determinism check: same input must produce same return code. */
  if (r1 != r2) {
    zr_fuzz_trap();
  }

  /* If valid, parsed headers must match. */
  if (r1 == ZR_OK) {
    if (memcmp(&v1.hdr, &v2.hdr, sizeof(v1.hdr)) != 0) {
      zr_fuzz_trap();
    }
  }
}

int main(void) {
  enum { kIters = 1000, kMaxSize = 512 };
  uint32_t seed = 0xD1A7B00u;
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
