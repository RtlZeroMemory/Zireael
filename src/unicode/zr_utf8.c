/*
  src/unicode/zr_utf8.c — Deterministic UTF-8 decoding primitives.

  Why: Centralizes the project's locked invalid UTF-8 policy so all Unicode
  operations (graphemes/width/wrap) can share a single fuzzable decoder.
*/

#include "unicode/zr_utf8.h"

#include <stdbool.h>

static zr_utf8_decode_result_t zr_utf8_invalid(size_t len) {
  zr_utf8_decode_result_t r;
  r.scalar = 0xFFFDu;
  r.valid = 0u;
  r._pad0 = 0u;
  r.size = (uint8_t)((len > 0u) ? 1u : 0u);
  return r;
}

static bool zr_utf8_is_cont(uint8_t b) { return (uint8_t)(b & 0xC0u) == 0x80u; }

/* Decode one UTF-8 codepoint; returns U+FFFD with valid=0 for invalid sequences. */
zr_utf8_decode_result_t zr_utf8_decode_one(const uint8_t* s, size_t len) {
  if (len == 0u || !s) {
    return zr_utf8_invalid(len);
  }

  const uint8_t b0 = s[0];

  /* 1-byte (ASCII). */
  if (b0 <= 0x7Fu) {
    zr_utf8_decode_result_t r;
    r.scalar = (uint32_t)b0;
    r.size = 1u;
    r.valid = 1u;
    r._pad0 = 0u;
    return r;
  }

  /* 2-byte: C2..DF 80..BF (C0/C1 are overlong prefixes). */
  if (b0 >= 0xC2u && b0 <= 0xDFu) {
    if (len < 2u) {
      return zr_utf8_invalid(len);
    }
    const uint8_t b1 = s[1];
    if (!zr_utf8_is_cont(b1)) {
      return zr_utf8_invalid(len);
    }

    const uint32_t cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
    zr_utf8_decode_result_t r;
    r.scalar = cp;
    r.size = 2u;
    r.valid = 1u;
    r._pad0 = 0u;
    return r;
  }

  /* 3-byte: E0..EF 80..BF 80..BF with overlong/surrogate constraints. */
  if (b0 >= 0xE0u && b0 <= 0xEFu) {
    if (len < 3u) {
      return zr_utf8_invalid(len);
    }
    const uint8_t b1 = s[1];
    const uint8_t b2 = s[2];
    if (!zr_utf8_is_cont(b1) || !zr_utf8_is_cont(b2)) {
      return zr_utf8_invalid(len);
    }

    /*
      Overlong + surrogate exclusions:
        - E0 A0..BF ..  => >= U+0800
        - ED 80..9F ..  => <= U+D7FF (avoid UTF-16 surrogate range)
    */
    if (b0 == 0xE0u && b1 < 0xA0u) {
      return zr_utf8_invalid(len);
    }
    if (b0 == 0xEDu && b1 > 0x9Fu) {
      return zr_utf8_invalid(len);
    }

    const uint32_t cp = ((uint32_t)(b0 & 0x0Fu) << 12) | ((uint32_t)(b1 & 0x3Fu) << 6) |
                        (uint32_t)(b2 & 0x3Fu);
    if (cp >= 0xD800u && cp <= 0xDFFFu) {
      return zr_utf8_invalid(len);
    }
    if (cp < 0x800u) {
      return zr_utf8_invalid(len);
    }

    zr_utf8_decode_result_t r;
    r.scalar = cp;
    r.size = 3u;
    r.valid = 1u;
    r._pad0 = 0u;
    return r;
  }

  /* 4-byte: F0..F4 80..BF 80..BF 80..BF with overlong/max constraints. */
  if (b0 >= 0xF0u && b0 <= 0xF4u) {
    if (len < 4u) {
      return zr_utf8_invalid(len);
    }
    const uint8_t b1 = s[1];
    const uint8_t b2 = s[2];
    const uint8_t b3 = s[3];
    if (!zr_utf8_is_cont(b1) || !zr_utf8_is_cont(b2) || !zr_utf8_is_cont(b3)) {
      return zr_utf8_invalid(len);
    }

    /*
      Overlong + max exclusions:
        - F0 90..BF .. .. => >= U+10000
        - F4 80..8F .. .. => <= U+10FFFF
    */
    if (b0 == 0xF0u && b1 < 0x90u) {
      return zr_utf8_invalid(len);
    }
    if (b0 == 0xF4u && b1 > 0x8Fu) {
      return zr_utf8_invalid(len);
    }

    const uint32_t cp = ((uint32_t)(b0 & 0x07u) << 18) | ((uint32_t)(b1 & 0x3Fu) << 12) |
                        ((uint32_t)(b2 & 0x3Fu) << 6) | (uint32_t)(b3 & 0x3Fu);
    if (cp > 0x10FFFFu) {
      return zr_utf8_invalid(len);
    }
    if (cp < 0x10000u) {
      return zr_utf8_invalid(len);
    }

    zr_utf8_decode_result_t r;
    r.scalar = cp;
    r.size = 4u;
    r.valid = 1u;
    r._pad0 = 0u;
    return r;
  }

  /* Invalid leading byte. */
  return zr_utf8_invalid(len);
}
