/*
  tests/fuzz/fuzz_drawlist_parser.c — Drawlist validator fuzz target (smoke-mode).
*/

#include "core/zr_drawlist.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

static void zr_fuzz_one(const uint8_t* data, size_t size) {
  zr_limits_t lim = zr_limits_default();
  lim.dl_max_total_bytes = (uint32_t)size;
  lim.dl_max_cmds = 64u;
  lim.dl_max_strings = 64u;
  lim.dl_max_blobs = 64u;
  lim.dl_max_clip_depth = 16u;
  lim.dl_max_text_run_segments = 64u;

  zr_dl_view_t v1;
  zr_dl_view_t v2;
  const zr_result_t r1 = zr_dl_validate(data, size, &lim, &v1);
  const zr_result_t r2 = zr_dl_validate(data, size, &lim, &v2);

  /* Determinism: same input -> same return code (and same decoded header if OK). */
  if (r1 != r2) {
    zr_fuzz_trap();
  }
  if (r1 == ZR_OK) {
    if (memcmp(&v1.hdr, &v2.hdr, sizeof(v1.hdr)) != 0) {
      zr_fuzz_trap();
    }
  }
}

int main(void) {
  enum { kIters = 500, kMaxSize = 256 };
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
