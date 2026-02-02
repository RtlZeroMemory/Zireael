/*
  src/util/zr_caps.c — Limits defaults + validation.

  Why: Enforces deterministic, non-zero cap defaults and rejects invalid inputs.
*/

#include "util/zr_caps.h"

/* Return sensible default limits for arena, drawlist, and clip depth. */
zr_limits_t zr_limits_default(void) {
  zr_limits_t l;
  l.arena_max_total_bytes = 4u * 1024u * 1024u;
  l.arena_initial_bytes = 64u * 1024u;
  l.out_max_bytes_per_frame = 256u * 1024u;
  l.dl_max_total_bytes = 256u * 1024u;
  l.dl_max_cmds = 4096u;
  l.dl_max_strings = 4096u;
  l.dl_max_blobs = 4096u;
  l.dl_max_clip_depth = 64u;
  l.dl_max_text_run_segments = 4096u;
  l.diff_max_damage_rects = 4096u;
  return l;
}

/* Validate that all limits are non-zero and internally consistent. */
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
  if (limits->out_max_bytes_per_frame == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (limits->dl_max_total_bytes == 0u || limits->dl_max_cmds == 0u || limits->dl_max_strings == 0u ||
      limits->dl_max_blobs == 0u || limits->dl_max_clip_depth == 0u ||
      limits->dl_max_text_run_segments == 0u || limits->diff_max_damage_rects == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return ZR_OK;
}
