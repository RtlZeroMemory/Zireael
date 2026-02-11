/*
  src/unicode/zr_width.c — Deterministic terminal column width policy.

  Why: Provides stable, cross-platform column widths for core rendering and
  wrapping without depending on system wcwidth/locale.
*/

#include "unicode/zr_width.h"

#include "unicode/zr_unicode_data.h"
#include "unicode/zr_utf8.h"

#include <stdbool.h>

enum {
  ZR_WIDTH_ASCII_CONTROL_MAX = 0x20u,
  ZR_WIDTH_ASCII_DEL = 0x7Fu,
  ZR_WIDTH_ASCII_HASH = 0x23u,
  ZR_WIDTH_ASCII_ASTERISK = 0x2Au,
  ZR_WIDTH_ASCII_DIGIT_0 = 0x30u,
  ZR_WIDTH_ASCII_DIGIT_9 = 0x39u,
  ZR_WIDTH_VARIATION_SELECTOR_16 = 0xFE0Fu,
  ZR_WIDTH_COMBINING_ENCLOSING_KEYCAP = 0x20E3u,
};

static bool zr_width_is_ascii_control(uint32_t scalar) {
  return scalar < ZR_WIDTH_ASCII_CONTROL_MAX || scalar == ZR_WIDTH_ASCII_DEL;
}

static bool zr_width_is_keycap_base(uint32_t scalar) {
  if (scalar == ZR_WIDTH_ASCII_HASH || scalar == ZR_WIDTH_ASCII_ASTERISK) {
    return true;
  }
  return scalar >= ZR_WIDTH_ASCII_DIGIT_0 && scalar <= ZR_WIDTH_ASCII_DIGIT_9;
}

typedef enum zr_width_keycap_state_t {
  ZR_WIDTH_KEYCAP_STATE_START = 0,
  ZR_WIDTH_KEYCAP_STATE_AFTER_BASE,
  ZR_WIDTH_KEYCAP_STATE_AFTER_BASE_VS16,
  ZR_WIDTH_KEYCAP_STATE_MATCHED,
  ZR_WIDTH_KEYCAP_STATE_INVALID
} zr_width_keycap_state_t;

/*
 * Track the keycap emoji grammar:
 *   [0-9#*] U+FE0F? U+20E3
 *
 * Why: These sequences can render as emoji even though the base scalar is not
 * Extended_Pictographic/Emoji_Presentation, so width policy must recognize
 * them explicitly to avoid narrow-vs-wide terminal drift.
 */
static zr_width_keycap_state_t zr_width_keycap_next(zr_width_keycap_state_t state, uint32_t scalar) {
  if (state == ZR_WIDTH_KEYCAP_STATE_START) {
    return zr_width_is_keycap_base(scalar) ? ZR_WIDTH_KEYCAP_STATE_AFTER_BASE : ZR_WIDTH_KEYCAP_STATE_INVALID;
  }
  if (state == ZR_WIDTH_KEYCAP_STATE_AFTER_BASE) {
    if (scalar == ZR_WIDTH_VARIATION_SELECTOR_16) {
      return ZR_WIDTH_KEYCAP_STATE_AFTER_BASE_VS16;
    }
    if (scalar == ZR_WIDTH_COMBINING_ENCLOSING_KEYCAP) {
      return ZR_WIDTH_KEYCAP_STATE_MATCHED;
    }
    return ZR_WIDTH_KEYCAP_STATE_INVALID;
  }
  if (state == ZR_WIDTH_KEYCAP_STATE_AFTER_BASE_VS16) {
    if (scalar == ZR_WIDTH_COMBINING_ENCLOSING_KEYCAP) {
      return ZR_WIDTH_KEYCAP_STATE_MATCHED;
    }
    return ZR_WIDTH_KEYCAP_STATE_INVALID;
  }
  return ZR_WIDTH_KEYCAP_STATE_INVALID;
}

/* Return terminal column width of a single codepoint (0, 1, or 2). */
uint8_t zr_width_codepoint(uint32_t scalar) {
  if (zr_width_is_ascii_control(scalar)) {
    return 0u;
  }

  const zr_gcb_class_t gcb = zr_unicode_gcb_class(scalar);
  if (gcb == ZR_GCB_CONTROL || gcb == ZR_GCB_CR || gcb == ZR_GCB_LF) {
    return 0u;
  }
  if (gcb == ZR_GCB_EXTEND || gcb == ZR_GCB_ZWJ) {
    return 0u;
  }

  if (zr_unicode_is_eaw_wide(scalar)) {
    return 2u;
  }

  return 1u;
}

/* Return terminal column width of a grapheme cluster, applying emoji width policy. */
uint8_t zr_width_grapheme_utf8(const uint8_t* bytes, size_t len, zr_width_policy_t policy) {
  if (len == 0u || !bytes) {
    return 0u;
  }

  uint8_t width = 0u;
  size_t off = 0u;
  bool has_emoji = false;
  zr_width_keycap_state_t keycap_state = ZR_WIDTH_KEYCAP_STATE_START;

  while (off < len) {
    zr_utf8_decode_result_t d = zr_utf8_decode_one(bytes + off, len - off);
    if (d.size == 0u) {
      break;
    }
    off += (size_t)d.size;

    const bool is_emoji = zr_unicode_is_extended_pictographic(d.scalar) || zr_unicode_is_emoji_presentation(d.scalar);
    if (is_emoji) {
      has_emoji = true;
    }
    keycap_state = zr_width_keycap_next(keycap_state, d.scalar);

    /*
      Emoji policy must be able to force emoji to narrow width even when the
      codepoint is EastAsianWidth=Wide. Treat emoji codepoints as width 1 for
      the per-codepoint accumulator and apply policy after scanning.
    */
    const uint8_t w = is_emoji ? 1u : zr_width_codepoint(d.scalar);
    if (w > width) {
      width = w;
    }
  }

  if (keycap_state == ZR_WIDTH_KEYCAP_STATE_MATCHED) {
    has_emoji = true;
  }

  if (has_emoji) {
    const uint8_t emoji_w = (policy == ZR_WIDTH_EMOJI_WIDE) ? 2u : 1u;
    if (emoji_w > width) {
      width = emoji_w;
    }
  }

  return width;
}
