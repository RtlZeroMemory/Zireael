/*
  tests/fuzz/fuzz_utf8_decode.c — UTF-8 decoder fuzz target (smoke-mode).

  Why: Ensures the decoder never crashes/hangs and always makes progress on
  arbitrary bytes without reading out of bounds.
*/

#include "unicode/zr_utf8.h"
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
