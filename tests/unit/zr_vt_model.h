/*
  tests/unit/zr_vt_model.h — Minimal VT/ANSI output model for renderer tests.

  Why: Golden/unit tests need a deterministic way to apply emitted VT bytes and
  verify that the renderer's tracked terminal state and resulting screen content
  match what a strict terminal would do for the supported subset (CUP/SGR/ED/scroll).
*/

#ifndef ZR_TESTS_UNIT_ZR_VT_MODEL_H_INCLUDED
#define ZR_TESTS_UNIT_ZR_VT_MODEL_H_INCLUDED

#include "core/zr_diff.h"
#include "core/zr_framebuffer.h"

#include "util/zr_result.h"

#include <stddef.h>
#include <stdint.h>

typedef struct zr_vt_model_t {
  uint32_t cols;
  uint32_t rows;

  /* Screen contents in engine cell representation (spaces + wide invariants). */
  zr_fb_t screen;
  zr_rect_t clip_stack[2];
  zr_fb_painter_t painter;

  /* Active scroll region (0-based inclusive). */
  uint32_t scroll_top;
  uint32_t scroll_bottom;

  /* Terminal state as inferred from applied output bytes. */
  zr_term_state_t ts;
} zr_vt_model_t;

zr_result_t zr_vt_model_init(zr_vt_model_t* m, uint32_t cols, uint32_t rows);
void zr_vt_model_release(zr_vt_model_t* m);

zr_result_t zr_vt_model_reset(zr_vt_model_t* m, const zr_fb_t* screen, const zr_term_state_t* ts);
zr_result_t zr_vt_model_apply(zr_vt_model_t* m, const uint8_t* bytes, size_t len);

static inline const zr_fb_t* zr_vt_model_screen(const zr_vt_model_t* m) { return m ? &m->screen : NULL; }
static inline const zr_term_state_t* zr_vt_model_term_state(const zr_vt_model_t* m) { return m ? &m->ts : NULL; }

#endif /* ZR_TESTS_UNIT_ZR_VT_MODEL_H_INCLUDED */

