/*
  src/util/zr_caps.c — Limits defaults + validation.

  Why: Enforces deterministic, non-zero cap defaults and rejects invalid inputs.
*/

#include "util/zr_caps.h"

zr_limits_t zr_limits_default(void) {
  zr_limits_t l;
  l.arena_max_total_bytes = 4u * 1024u * 1024u;
  l.arena_initial_bytes = 64u * 1024u;
  return l;
}

zr_result_t zr_limits_validate(const zr_limits_t* limits) {
  if (!limits) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (limits->arena_max_total_bytes == 0u || limits->arena_initial_bytes == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (limits->arena_initial_bytes > limits->arena_max_total_bytes) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return ZR_OK;
}

