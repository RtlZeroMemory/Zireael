/*
  src/core/zr_fb.h — Compatibility shim for framebuffer users.

  Why: Older modules/tests include core/zr_fb.h. The framebuffer module moved to
  core/zr_framebuffer.h; this header keeps include paths working while code is
  migrated to the new API.
*/

#ifndef ZR_CORE_ZR_FB_H_INCLUDED
#define ZR_CORE_ZR_FB_H_INCLUDED

#include "core/zr_framebuffer.h"

#include <stdint.h>

typedef zr_cell_t zr_fb_cell_t;
typedef zr_rect_t zr_fb_rect_i32_t;

#endif /* ZR_CORE_ZR_FB_H_INCLUDED */
