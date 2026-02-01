/*
  src/core/zr_debug_overlay.h — Deterministic debug overlay renderer (core-internal).

  Why: Renders a small, bounded ASCII metrics overlay into the framebuffer in a
  way that preserves wide-glyph continuation invariants and never allocates.
*/

#ifndef ZR_CORE_ZR_DEBUG_OVERLAY_H_INCLUDED
#define ZR_CORE_ZR_DEBUG_OVERLAY_H_INCLUDED

#include "core/zr_framebuffer.h"
#include "core/zr_metrics.h"

#include "util/zr_result.h"

/* Deterministic overlay bounds (v1). */
#define ZR_DEBUG_OVERLAY_MAX_ROWS 4u
#define ZR_DEBUG_OVERLAY_MAX_COLS 40u

/*
  zr_debug_overlay_render:
    - Deterministically renders up to 4x40 cells at the top-left of fb.
    - Clip-safe: never writes outside fb bounds and avoids breaking wide glyphs
      that span the overlay boundary.
    - Never allocates; intended for engine-thread use.
*/
zr_result_t zr_debug_overlay_render(zr_fb_t* fb, const zr_metrics_t* metrics);

#endif /* ZR_CORE_ZR_DEBUG_OVERLAY_H_INCLUDED */
