/*
  src/core/zr_base64.h — Deterministic RFC4648 base64 encoder helpers.

  Why: Kitty/iTerm2 image protocols need stable base64 output without heap
  allocation in hot paths.
*/

#ifndef ZR_CORE_ZR_BASE64_H_INCLUDED
#define ZR_CORE_ZR_BASE64_H_INCLUDED

#include "util/zr_result.h"

#include <stddef.h>
#include <stdint.h>

/* Compute encoded size with overflow reporting (0 when overflow is set). */
size_t zr_base64_encoded_size(size_t in_len, uint8_t* out_overflow);

/* Encode input bytes to base64 (RFC4648 alphabet with '=' padding). */
zr_result_t zr_base64_encode(const uint8_t* in, size_t in_len, uint8_t* out, size_t out_cap, size_t* out_len);

#endif /* ZR_CORE_ZR_BASE64_H_INCLUDED */
