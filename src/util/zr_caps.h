/*
  src/util/zr_caps.h — Deterministic default limits and validation.

  Why: Centralizes cap defaults and validation so higher layers can be
  deterministic and reject invalid configurations.
*/

#ifndef ZR_UTIL_ZR_CAPS_H_INCLUDED
#define ZR_UTIL_ZR_CAPS_H_INCLUDED

#include "util/zr_result.h"

#include <stdint.h>

/*
  zr_limits_t:
    - Fields are deterministic caps that must be non-zero.
    - Validation rejects zeros and obvious invalid ranges.

  Note: This struct is expected to grow as new modules land.
*/
typedef struct zr_limits_t {
  uint32_t arena_max_total_bytes;
  uint32_t arena_initial_bytes;

  /*
    out_max_bytes_per_frame:
      - Maximum bytes the engine may emit in a single engine_present() call.
      - Enforced by allocating a fixed-size engine-owned output buffer and
        building diff output into it before a single platform flush.
  */
  uint32_t out_max_bytes_per_frame;

  /* Drawlist (v1) caps. */
  uint32_t dl_max_total_bytes;
  uint32_t dl_max_cmds;
  uint32_t dl_max_strings;
  uint32_t dl_max_blobs;
  uint32_t dl_max_clip_depth;
  uint32_t dl_max_text_run_segments;
} zr_limits_t;

zr_limits_t zr_limits_default(void);
zr_result_t zr_limits_validate(const zr_limits_t* limits);

#endif /* ZR_UTIL_ZR_CAPS_H_INCLUDED */
