/*
  include/zr/zr_terminal_caps.h — Runtime terminal capability snapshot (public ABI).

  Why: Exposes the engine’s conservative, backend-discovered output capabilities
  to wrappers without leaking OS headers. This is a fixed-width POD struct.
*/

#ifndef ZR_ZR_TERMINAL_CAPS_H_INCLUDED
#define ZR_ZR_TERMINAL_CAPS_H_INCLUDED

#include "zr/zr_platform_types.h"

#include <stdint.h>

typedef struct zr_terminal_caps_t {
  plat_color_mode_t color_mode;
  uint8_t           supports_mouse;
  uint8_t           supports_bracketed_paste;
  uint8_t           supports_focus_events;
  uint8_t           supports_osc52;
  uint8_t           supports_sync_update;
  uint8_t           supports_scroll_region;
  uint8_t           supports_cursor_shape;
  uint8_t           supports_output_wait_writable;
  uint8_t           _pad0[3];

  uint32_t          sgr_attrs_supported;
} zr_terminal_caps_t;

#endif /* ZR_ZR_TERMINAL_CAPS_H_INCLUDED */
