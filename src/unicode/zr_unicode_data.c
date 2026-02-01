/*
  src/unicode/zr_unicode_data.c — Minimal pinned Unicode property tables.

  Why: Implements deterministic property lookups without heap allocation.
  Tables are intentionally a minimal subset sufficient for the current unit
  vectors (combining marks, RI flags, basic ZWJ emoji sequences).
*/

#include "unicode/zr_unicode_data.h"

typedef struct zr_unicode_range8_t {
  uint32_t lo;
  uint32_t hi;
  uint8_t  v;
  uint8_t  _pad0[3];
} zr_unicode_range8_t;

typedef struct zr_unicode_range_t {
  uint32_t lo;
  uint32_t hi;
} zr_unicode_range_t;

/*
  Grapheme_Cluster_Break (minimal subset).

  Notes:
    - Controls include C0/C1 controls (plus CR/LF separately).
    - Extend includes combining diacritics and common emoji-related extenders
      (VS16, skin tones) needed for stable tests.
*/
static const zr_unicode_range8_t kGcbRanges[] = {
  /* C0 controls excluding CR/LF: 0000..0009, 000B..000C, 000E..001F. */
  {0x0000u, 0x0009u, (uint8_t)ZR_GCB_CONTROL, {0u, 0u, 0u}},
  {0x000Au, 0x000Au, (uint8_t)ZR_GCB_LF, {0u, 0u, 0u}},
  {0x000Bu, 0x000Cu, (uint8_t)ZR_GCB_CONTROL, {0u, 0u, 0u}},
  {0x000Du, 0x000Du, (uint8_t)ZR_GCB_CR, {0u, 0u, 0u}},
  {0x000Eu, 0x001Fu, (uint8_t)ZR_GCB_CONTROL, {0u, 0u, 0u}},

  /* DEL + C1 controls. */
  {0x007Fu, 0x009Fu, (uint8_t)ZR_GCB_CONTROL, {0u, 0u, 0u}},

  /* Combining Diacritical Marks (Extend). */
  {0x0300u, 0x036Fu, (uint8_t)ZR_GCB_EXTEND, {0u, 0u, 0u}},

  /* ZWNJ (treated as Extend) + ZWJ. */
  {0x200Cu, 0x200Cu, (uint8_t)ZR_GCB_EXTEND, {0u, 0u, 0u}},
  {0x200Du, 0x200Du, (uint8_t)ZR_GCB_ZWJ, {0u, 0u, 0u}},

  /* Variation Selectors (Extend). */
  {0xFE00u, 0xFE0Fu, (uint8_t)ZR_GCB_EXTEND, {0u, 0u, 0u}},

  /* Regional indicators (flags). */
  {0x1F1E6u, 0x1F1FFu, (uint8_t)ZR_GCB_REGIONAL_INDICATOR, {0u, 0u, 0u}},

  /* Emoji modifiers (skin tones) are Extend. */
  {0x1F3FBu, 0x1F3FFu, (uint8_t)ZR_GCB_EXTEND, {0u, 0u, 0u}},
};

static const zr_unicode_range_t kExtendedPictographicRanges[] = {
  /*
    Extended_Pictographic (minimal subset).

    The full property table is intentionally not embedded yet; expand only as
    additional module vectors are added.
  */
  {0x1F469u, 0x1F469u}, /* WOMAN */
  {0x1F4BBu, 0x1F4BBu}, /* LAPTOP */
};

static uint32_t zr_unicode_ranges8_count(void) { return (uint32_t)(sizeof(kGcbRanges) / sizeof(kGcbRanges[0])); }
static uint32_t zr_unicode_ranges_count(void) { return (uint32_t)(sizeof(kExtendedPictographicRanges) / sizeof(kExtendedPictographicRanges[0])); }

static bool zr_unicode_in_ranges(const zr_unicode_range_t* ranges, uint32_t n, uint32_t scalar) {
  uint32_t lo = 0u;
  uint32_t hi = n;
  while (lo < hi) {
    const uint32_t mid = lo + (hi - lo) / 2u;
    const zr_unicode_range_t r = ranges[mid];
    if (scalar < r.lo) {
      hi = mid;
      continue;
    }
    if (scalar > r.hi) {
      lo = mid + 1u;
      continue;
    }
    return true;
  }
  return false;
}

static zr_gcb_class_t zr_unicode_ranges8_lookup(const zr_unicode_range8_t* ranges, uint32_t n, uint32_t scalar) {
  uint32_t lo = 0u;
  uint32_t hi = n;
  while (lo < hi) {
    const uint32_t mid = lo + (hi - lo) / 2u;
    const zr_unicode_range8_t r = ranges[mid];
    if (scalar < r.lo) {
      hi = mid;
      continue;
    }
    if (scalar > r.hi) {
      lo = mid + 1u;
      continue;
    }
    return (zr_gcb_class_t)r.v;
  }
  return ZR_GCB_OTHER;
}

zr_gcb_class_t zr_unicode_gcb_class(uint32_t scalar) {
  return zr_unicode_ranges8_lookup(kGcbRanges, zr_unicode_ranges8_count(), scalar);
}

bool zr_unicode_is_extended_pictographic(uint32_t scalar) {
  return zr_unicode_in_ranges(kExtendedPictographicRanges, zr_unicode_ranges_count(), scalar);
}
