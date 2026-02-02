/*
  tests/fuzz/zr_fuzz_smoke.c — Fuzz scaffolding (portable smoke-mode driver).

  Why: Provides a deterministic, time-bounded-ish fuzz driver that runs without
  libFuzzer. Future fuzz targets can follow this pattern and optionally switch
  to a libFuzzer entrypoint.
*/

#include <stddef.h>
#include <stdint.h>

static uint32_t zr_xorshift32(uint32_t* state) {
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

/*
  Replace this body with a real target as modules land.

  Contract for future targets:
    - must never crash/hang
    - must treat input as untrusted bytes (bounds check everything)
*/
static int zr_fuzz_target_one_input(const uint8_t* data, size_t size) {
  (void)data;
  (void)size;
  return 0;
}

int main(void) {
  /* Deterministic run: fixed iteration count and PRNG seed. */
  enum { kIters = 200, kMaxSize = 256 };
  uint32_t seed = 0xC0FFEEu;
  uint8_t buf[kMaxSize];

  for (int i = 0; i < kIters; i++) {
    const size_t sz = (size_t)(zr_xorshift32(&seed) % (uint32_t)kMaxSize);
    for (size_t j = 0; j < sz; j++) {
      buf[j] = (uint8_t)(zr_xorshift32(&seed) & 0xFFu);
    }
    (void)zr_fuzz_target_one_input(buf, sz);
  }

  return 0;
}

