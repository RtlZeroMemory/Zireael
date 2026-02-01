/*
  src/unicode/zr_grapheme.c — Deterministic grapheme cluster iteration (UAX #29 subset).

  Why: Enables grapheme-safe width and wrapping with a strict progress and
  bounds-safety contract, even for malformed UTF-8.
*/

#include "unicode/zr_grapheme.h"

#include "unicode/zr_unicode_data.h"
#include "unicode/zr_utf8.h"

static bool zr_grapheme_is_control(zr_gcb_class_t c) {
  return c == ZR_GCB_CONTROL || c == ZR_GCB_CR || c == ZR_GCB_LF;
}

static bool zr_grapheme_is_hangul_l(zr_gcb_class_t c) { return c == ZR_GCB_L; }
static bool zr_grapheme_is_hangul_v(zr_gcb_class_t c) { return c == ZR_GCB_V; }
static bool zr_grapheme_is_hangul_t(zr_gcb_class_t c) { return c == ZR_GCB_T; }
static bool zr_grapheme_is_hangul_lv(zr_gcb_class_t c) { return c == ZR_GCB_LV; }
static bool zr_grapheme_is_hangul_lvt(zr_gcb_class_t c) { return c == ZR_GCB_LVT; }

static bool zr_grapheme_should_break(zr_gcb_class_t prev_class, bool prev_zwj_after_ep, uint32_t ri_run,
                                     zr_gcb_class_t next_class, bool next_is_ep) {
  /*
    Implemented UAX #29 rules (Unicode 15.1.0, core set):
      - GB3: CR x LF
      - GB4/GB5: break around controls
      - GB6/7/8: Hangul syllable sequences
      - GB9/9a/9b/9c: x Extend / SpacingMark / Prepend x / x ZWJ
      - GB11: EP Extend* ZWJ x EP
      - GB12/GB13: RI pairs
      - otherwise: break
  */

  if (prev_class == ZR_GCB_CR && next_class == ZR_GCB_LF) {
    return false;
  }

  if (zr_grapheme_is_control(prev_class)) {
    return true;
  }
  if (zr_grapheme_is_control(next_class)) {
    return true;
  }

  /* GB6: L x (L|V|LV|LVT) */
  if (zr_grapheme_is_hangul_l(prev_class) &&
      (zr_grapheme_is_hangul_l(next_class) || zr_grapheme_is_hangul_v(next_class) || zr_grapheme_is_hangul_lv(next_class) ||
       zr_grapheme_is_hangul_lvt(next_class))) {
    return false;
  }

  /* GB7: (LV|V) x (V|T) */
  if ((zr_grapheme_is_hangul_lv(prev_class) || zr_grapheme_is_hangul_v(prev_class)) &&
      (zr_grapheme_is_hangul_v(next_class) || zr_grapheme_is_hangul_t(next_class))) {
    return false;
  }

  /* GB8: (LVT|T) x T */
  if ((zr_grapheme_is_hangul_lvt(prev_class) || zr_grapheme_is_hangul_t(prev_class)) && zr_grapheme_is_hangul_t(next_class)) {
    return false;
  }

  /* GB9: x Extend */
  if (next_class == ZR_GCB_EXTEND) {
    return false;
  }

  /* GB9a: x SpacingMark */
  if (next_class == ZR_GCB_SPACINGMARK) {
    return false;
  }

  /* GB9b: Prepend x */
  if (prev_class == ZR_GCB_PREPEND) {
    return false;
  }

  /* GB9c: x ZWJ */
  if (next_class == ZR_GCB_ZWJ) {
    return false;
  }

  /* GB11: ... ZWJ x EP when ZWJ is preceded by EP (ignoring Extend). */
  if (prev_class == ZR_GCB_ZWJ && next_is_ep && prev_zwj_after_ep) {
    return false;
  }

  /* GB12/13: Pair regional indicators. */
  if (prev_class == ZR_GCB_REGIONAL_INDICATOR && next_class == ZR_GCB_REGIONAL_INDICATOR) {
    /*
      Within a run of RI, group into pairs:
        RI RI | RI RI | ...
      No-break when the count of RI seen in the current cluster so far is odd.
    */
    return (ri_run % 2u) == 0u;
  }

  return true;
}

void zr_grapheme_iter_init(zr_grapheme_iter_t* it, const uint8_t* bytes, size_t len) {
  if (!it) {
    return;
  }
  it->bytes = bytes;
  it->len = len;
  it->off = 0u;
}

/* Advance iterator to next grapheme cluster; returns false when exhausted. */
bool zr_grapheme_next(zr_grapheme_iter_t* it, zr_grapheme_t* out) {
  if (!it || !out) {
    return false;
  }
  if (it->off >= it->len) {
    return false;
  }
  if (!it->bytes) {
    return false;
  }

  const size_t start = it->off;

  zr_utf8_decode_result_t prev_dec = zr_utf8_decode_one(it->bytes + it->off, it->len - it->off);
  if (prev_dec.size == 0u) {
    return false;
  }
  it->off += (size_t)prev_dec.size;

  zr_gcb_class_t prev_class = zr_unicode_gcb_class(prev_dec.scalar);
  bool          prev_is_ep = zr_unicode_is_extended_pictographic(prev_dec.scalar);

  uint32_t ri_run = (prev_class == ZR_GCB_REGIONAL_INDICATOR) ? 1u : 0u;

  /*
   * GB11 state tracking (emoji ZWJ sequences):
   *
   * Rule GB11: ExtPict Extend* ZWJ × ExtPict
   *   "Don't break between ZWJ and Extended_Pictographic when the ZWJ
   *    is preceded by Extended_Pictographic (ignoring Extend chars)."
   *
   * We track:
   *   - last_non_extend_is_ep: Was the last non-Extend codepoint an ExtPict?
   *   - prev_zwj_after_ep: Is the previous char ZWJ that came after ExtPict?
   *
   * Example: 👨 + Extend + ZWJ + 👩 → single grapheme cluster
   */
  bool last_non_extend_is_ep = false;
  if (prev_class != ZR_GCB_EXTEND) {
    last_non_extend_is_ep = prev_is_ep;
  }

  bool prev_zwj_after_ep = false;
  if (prev_class == ZR_GCB_ZWJ) {
    prev_zwj_after_ep = last_non_extend_is_ep;
  }

  while (it->off < it->len) {
    const size_t next_off = it->off;
    zr_utf8_decode_result_t next_dec = zr_utf8_decode_one(it->bytes + next_off, it->len - next_off);
    if (next_dec.size == 0u) {
      break;
    }

    const zr_gcb_class_t next_class = zr_unicode_gcb_class(next_dec.scalar);
    const bool next_is_ep = zr_unicode_is_extended_pictographic(next_dec.scalar);

    if (zr_grapheme_should_break(prev_class, prev_zwj_after_ep, ri_run, next_class, next_is_ep)) {
      break;
    }

    it->off += (size_t)next_dec.size;

    if (next_class == ZR_GCB_REGIONAL_INDICATOR) {
      ri_run++;
    } else {
      ri_run = 0u;
    }

    /*
      Update GB11 state:
        - prev_zwj_after_ep is a property of the "prev" codepoint when it is ZWJ.
        - last_non_extend_is_ep tracks whether the last non-Extend codepoint was EP.
    */
    prev_zwj_after_ep = false;
    if (next_class == ZR_GCB_ZWJ) {
      prev_zwj_after_ep = last_non_extend_is_ep;
    }
    if (next_class != ZR_GCB_EXTEND) {
      last_non_extend_is_ep = next_is_ep;
    }

    prev_class = next_class;
  }

  out->offset = start;
  out->size = it->off - start;
  return true;
}
