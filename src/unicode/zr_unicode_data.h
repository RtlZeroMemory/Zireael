/*
  src/unicode/zr_unicode_data.h — Minimal pinned Unicode property tables.

  Why: Grapheme segmentation and emoji width policy require stable Unicode
  property lookups. Full Unicode tables are intentionally not included yet;
  this file provides a deterministic minimal subset used by the current unit
  vectors (Extend/ZWJ/RI/EP/control).
*/

#ifndef ZR_UNICODE_ZR_UNICODE_DATA_H_INCLUDED
#define ZR_UNICODE_ZR_UNICODE_DATA_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

typedef enum zr_gcb_class_t {
  ZR_GCB_OTHER = 0,
  ZR_GCB_CR,
  ZR_GCB_LF,
  ZR_GCB_CONTROL,
  ZR_GCB_EXTEND,
  ZR_GCB_ZWJ,
  ZR_GCB_REGIONAL_INDICATOR
} zr_gcb_class_t;

zr_gcb_class_t zr_unicode_gcb_class(uint32_t scalar);
bool           zr_unicode_is_extended_pictographic(uint32_t scalar);

#endif /* ZR_UNICODE_ZR_UNICODE_DATA_H_INCLUDED */

