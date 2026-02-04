/*
  examples/zr_example_common.h — Small helpers for example binaries.

  Why: Keeps the examples focused on the public ABI (drawlist/event bytes) by
  centralizing little-endian reads/writes and 4-byte alignment helpers.
*/

#ifndef ZR_EXAMPLE_COMMON_H_INCLUDED
#define ZR_EXAMPLE_COMMON_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

/* --- Little-endian helpers (on-buffer ABI is little-endian) --- */

static inline void zr_ex_le16_write(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v);
  p[1] = (uint8_t)(v >> 8u);
}

static inline void zr_ex_le32_write(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v);
  p[1] = (uint8_t)(v >> 8u);
  p[2] = (uint8_t)(v >> 16u);
  p[3] = (uint8_t)(v >> 24u);
}

static inline uint32_t zr_ex_le32_read(const uint8_t* p) {
  return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8u) | ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

/* --- Alignment --- */

static inline uint32_t zr_ex_align4_u32(uint32_t x) { return (x + 3u) & ~3u; }

/* --- Color --- */

static inline uint32_t zr_ex_rgb_u32(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16u) | ((uint32_t)g << 8u) | (uint32_t)b;
}

#endif /* ZR_EXAMPLE_COMMON_H_INCLUDED */

