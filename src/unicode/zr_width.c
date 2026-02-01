/*
  src/unicode/zr_width.c — Deterministic terminal column width policy.

  Why: Provides stable, cross-platform column widths for core rendering and
  wrapping without depending on system wcwidth/locale.
*/

#include "unicode/zr_width.h"

#include "unicode/zr_unicode_data.h"
#include "unicode/zr_utf8.h"

#include <stdbool.h>

static bool zr_width_is_ascii_control(uint32_t scalar) {
  return scalar < 0x20u || scalar == 0x7Fu;
}

static bool zr_width_is_cjk_wide(uint32_t scalar) {
  /*
    Minimal pinned wide ranges sufficient for unit vectors:
      - CJK Unified Ideographs
      - Hangul syllables
      - Hiragana / Katakana
      - Fullwidth forms
  */
  if (scalar >= 0x4E00u && scalar <= 0x9FFFu) {
    return true;
  }
  if (scalar >= 0xAC00u && scalar <= 0xD7A3u) {
    return true;
  }
  if (scalar >= 0x3040u && scalar <= 0x30FFu) {
    return true;
  }
  if (scalar >= 0xFF01u && scalar <= 0xFF60u) {
    return true;
  }
  return false;
}

uint8_t zr_width_codepoint(uint32_t scalar) {
  if (zr_width_is_ascii_control(scalar)) {
    return 0u;
  }

  const zr_gcb_class_t gcb = zr_unicode_gcb_class(scalar);
  if (gcb == ZR_GCB_CONTROL || gcb == ZR_GCB_CR || gcb == ZR_GCB_LF) {
    return 0u;
  }
  if (gcb == ZR_GCB_EXTEND) {
    return 0u;
  }

  if (zr_width_is_cjk_wide(scalar)) {
    return 2u;
  }

  return 1u;
}

uint8_t zr_width_grapheme_utf8(const uint8_t* bytes, size_t len, zr_width_policy_t policy) {
  if (len == 0u || bytes == NULL) {
    return 0u;
  }

  uint8_t  width = 0u;
  size_t   off = 0u;
  bool     has_ep = false;

  while (off < len) {
    zr_utf8_decode_result_t d = zr_utf8_decode_one(bytes + off, len - off);
    if (d.size == 0u) {
      break;
    }
    off += (size_t)d.size;

    if (zr_unicode_is_extended_pictographic(d.scalar)) {
      has_ep = true;
    }

    const uint8_t w = zr_width_codepoint(d.scalar);
    if (w > width) {
      width = w;
    }
  }

  if (has_ep) {
    const uint8_t ep_w = (policy == ZR_WIDTH_EMOJI_WIDE) ? 2u : 1u;
    if (ep_w > width) {
      width = ep_w;
    }
  }

  return width;
}
