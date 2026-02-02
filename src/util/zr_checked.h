/*
  src/util/zr_checked.h — Overflow-safe arithmetic helpers.

  Why: Centralizes checked size/offset math to avoid undefined behavior and to
  keep "no partial effects" contracts easy to maintain.
*/

#ifndef ZR_UTIL_ZR_CHECKED_H_INCLUDED
#define ZR_UTIL_ZR_CHECKED_H_INCLUDED

/*
 * Checked arithmetic helpers for size_t and uint32_t.
 *
 * Pattern:
 *   - Returns true on success, false on overflow
 *   - On failure, *out is NOT modified (enables chaining without partial writes)
 *   - NULL out pointer returns false
 *
 * Usage:
 *   size_t total;
 *   if (!zr_checked_add_size(a, b, &total)) return ZR_ERR_LIMIT;
 *   if (!zr_checked_mul_size(total, c, &total)) return ZR_ERR_LIMIT;
 *   // total is valid only if both checks passed
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline bool zr_is_pow2_size(size_t v) {
  return (v != 0u) && ((v & (v - 1u)) == 0u);
}

static inline bool zr_checked_add_size(size_t a, size_t b, size_t* out) {
  if (!out) {
    return false;
  }
  if (a > (SIZE_MAX - b)) {
    return false;
  }
  *out = a + b;
  return true;
}

static inline bool zr_checked_sub_size(size_t a, size_t b, size_t* out) {
  if (!out) {
    return false;
  }
  if (a < b) {
    return false;
  }
  *out = a - b;
  return true;
}

static inline bool zr_checked_mul_size(size_t a, size_t b, size_t* out) {
  if (!out) {
    return false;
  }
  if (a == 0u || b == 0u) {
    *out = 0u;
    return true;
  }
  if (a > (SIZE_MAX / b)) {
    return false;
  }
  *out = a * b;
  return true;
}

static inline bool zr_checked_add_u32(uint32_t a, uint32_t b, uint32_t* out) {
  if (!out) {
    return false;
  }
  if (a > (UINT32_MAX - b)) {
    return false;
  }
  *out = a + b;
  return true;
}

static inline bool zr_checked_mul_u32(uint32_t a, uint32_t b, uint32_t* out) {
  if (!out) {
    return false;
  }
  if (a == 0u || b == 0u) {
    *out = 0u;
    return true;
  }
  if (a > (UINT32_MAX / b)) {
    return false;
  }
  *out = a * b;
  return true;
}

static inline bool zr_checked_align_up_size(size_t value, size_t align, size_t* out) {
  if (!out) {
    return false;
  }
  if (align == 0u || !zr_is_pow2_size(align)) {
    return false;
  }
  const size_t mask = align - 1u;
  if (value > (SIZE_MAX - mask)) {
    return false;
  }
  *out = (value + mask) & ~mask;
  return true;
}

#endif /* ZR_UTIL_ZR_CHECKED_H_INCLUDED */
