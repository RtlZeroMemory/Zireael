/*
  src/core/zr_diff.h — Pure framebuffer diff renderer to VT/ANSI bytes.

  Why: Computes deterministic terminal output bytes for prev→next framebuffer
  changes under pinned capabilities and an assumed initial terminal state.
*/

#ifndef ZR_CORE_ZR_DIFF_H_INCLUDED
#define ZR_CORE_ZR_DIFF_H_INCLUDED

#include "core/zr_framebuffer.h"
#include "platform/zr_platform.h"
#include "util/zr_result.h"

#include <stddef.h>
#include <stdint.h>

typedef struct zr_term_state_t {
  /* 0-based cursor position in character cells. */
  uint32_t   cursor_x;
  uint32_t   cursor_y;
  zr_style_t style;
} zr_term_state_t;

typedef struct zr_diff_stats_t {
  uint32_t dirty_lines;
  uint32_t dirty_cells;
  size_t   bytes_emitted;
} zr_diff_stats_t;

/*
  zr_diff_render:
    - Pure function: does not mutate framebuffers.
    - On success:
        - returns ZR_OK
        - writes [0..*out_len) bytes to out_buf
        - writes final terminal state to out_final_term_state
        - writes stats to out_stats
    - On failure:
        - returns a negative ZR_ERR_*
        - sets *out_len = 0
        - zeroes out_final_term_state and out_stats
        - out_buf contents are unspecified (caller must respect *out_len)
*/
zr_result_t zr_diff_render(const zr_fb_t* prev,
                           const zr_fb_t* next,
                           const plat_caps_t* caps,
                           const zr_term_state_t* initial_term_state,
                           uint8_t* out_buf,
                           size_t out_cap,
                           size_t* out_len,
                           zr_term_state_t* out_final_term_state,
                           zr_diff_stats_t* out_stats);

#endif /* ZR_CORE_ZR_DIFF_H_INCLUDED */
