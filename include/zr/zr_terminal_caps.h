/*
  include/zr/zr_terminal_caps.h — Runtime terminal capability snapshot (public ABI).

  Why: Exposes the engine’s conservative, backend-discovered output capabilities
  to wrappers without leaking OS headers. This is a fixed-width POD surface that
  includes both legacy caps and an extended detection profile.
*/

#ifndef ZR_ZR_TERMINAL_CAPS_H_INCLUDED
#define ZR_ZR_TERMINAL_CAPS_H_INCLUDED

#include "zr/zr_platform_types.h"

#include <stdint.h>

typedef enum zr_terminal_id_t {
  ZR_TERM_UNKNOWN = 0,
  ZR_TERM_KITTY,
  ZR_TERM_GHOSTTY,
  ZR_TERM_WEZTERM,
  ZR_TERM_FOOT,
  ZR_TERM_ITERM2,
  ZR_TERM_VTE,
  ZR_TERM_KONSOLE,
  ZR_TERM_CONTOUR,
  ZR_TERM_WINDOWS_TERMINAL,
  ZR_TERM_ALACRITTY,
  ZR_TERM_XTERM,
  ZR_TERM_MINTTY,
  ZR_TERM_TMUX,
  ZR_TERM_SCREEN,
  ZR_TERM_COUNT
} zr_terminal_id_t;

typedef uint32_t zr_terminal_cap_flags_t;

/* --- Extended terminal capability flags (for profile + force/suppress overrides) --- */
#define ZR_TERM_CAP_SIXEL ((zr_terminal_cap_flags_t)(1u << 0u))
#define ZR_TERM_CAP_KITTY_GRAPHICS ((zr_terminal_cap_flags_t)(1u << 1u))
#define ZR_TERM_CAP_ITERM2_IMAGES ((zr_terminal_cap_flags_t)(1u << 2u))
#define ZR_TERM_CAP_UNDERLINE_STYLES ((zr_terminal_cap_flags_t)(1u << 3u))
#define ZR_TERM_CAP_COLORED_UNDERLINES ((zr_terminal_cap_flags_t)(1u << 4u))
#define ZR_TERM_CAP_HYPERLINKS ((zr_terminal_cap_flags_t)(1u << 5u))
#define ZR_TERM_CAP_GRAPHEME_CLUSTERS ((zr_terminal_cap_flags_t)(1u << 6u))
#define ZR_TERM_CAP_OVERLINE ((zr_terminal_cap_flags_t)(1u << 7u))
#define ZR_TERM_CAP_PIXEL_MOUSE ((zr_terminal_cap_flags_t)(1u << 8u))
#define ZR_TERM_CAP_KITTY_KEYBOARD ((zr_terminal_cap_flags_t)(1u << 9u))

/* --- Legacy/backend caps exposed through engine_get_caps() --- */
#define ZR_TERM_CAP_MOUSE ((zr_terminal_cap_flags_t)(1u << 10u))
#define ZR_TERM_CAP_BRACKETED_PASTE ((zr_terminal_cap_flags_t)(1u << 11u))
#define ZR_TERM_CAP_FOCUS_EVENTS ((zr_terminal_cap_flags_t)(1u << 12u))
#define ZR_TERM_CAP_OSC52 ((zr_terminal_cap_flags_t)(1u << 13u))
#define ZR_TERM_CAP_SYNC_UPDATE ((zr_terminal_cap_flags_t)(1u << 14u))
#define ZR_TERM_CAP_SCROLL_REGION ((zr_terminal_cap_flags_t)(1u << 15u))
#define ZR_TERM_CAP_CURSOR_SHAPE ((zr_terminal_cap_flags_t)(1u << 16u))
#define ZR_TERM_CAP_OUTPUT_WAIT_WRITABLE ((zr_terminal_cap_flags_t)(1u << 17u))

#define ZR_TERM_CAP_ALL_MASK                                                                                           \
  ((zr_terminal_cap_flags_t)(ZR_TERM_CAP_SIXEL | ZR_TERM_CAP_KITTY_GRAPHICS | ZR_TERM_CAP_ITERM2_IMAGES |              \
                             ZR_TERM_CAP_UNDERLINE_STYLES | ZR_TERM_CAP_COLORED_UNDERLINES | ZR_TERM_CAP_HYPERLINKS |  \
                             ZR_TERM_CAP_GRAPHEME_CLUSTERS | ZR_TERM_CAP_OVERLINE | ZR_TERM_CAP_PIXEL_MOUSE |          \
                             ZR_TERM_CAP_KITTY_KEYBOARD | ZR_TERM_CAP_MOUSE | ZR_TERM_CAP_BRACKETED_PASTE |            \
                             ZR_TERM_CAP_FOCUS_EVENTS | ZR_TERM_CAP_OSC52 | ZR_TERM_CAP_SYNC_UPDATE |                  \
                             ZR_TERM_CAP_SCROLL_REGION | ZR_TERM_CAP_CURSOR_SHAPE | ZR_TERM_CAP_OUTPUT_WAIT_WRITABLE))

/*
  Extended terminal profile (read-only engine snapshot).

  Notes:
    - `version_string` stores raw XTVERSION payload text (truncated + NUL).
    - Pixel metrics are zero when unknown.
    - *_responded flags track probe response presence, not support.
*/
typedef struct zr_terminal_profile_t {
  zr_terminal_id_t id;
  uint8_t _pad0[3];

  char version_string[64];

  uint8_t supports_sixel;
  uint8_t supports_kitty_graphics;
  uint8_t supports_iterm2_images;
  uint8_t supports_underline_styles;
  uint8_t supports_colored_underlines;
  uint8_t supports_hyperlinks;
  uint8_t supports_grapheme_clusters;
  uint8_t supports_overline;

  uint8_t supports_pixel_mouse;
  uint8_t supports_kitty_keyboard;
  uint8_t supports_mouse;
  uint8_t supports_bracketed_paste;
  uint8_t supports_focus_events;
  uint8_t supports_osc52;
  uint8_t supports_sync_update;
  uint8_t _pad1;

  uint16_t cell_width_px;
  uint16_t cell_height_px;
  uint16_t screen_width_px;
  uint16_t screen_height_px;

  uint8_t xtversion_responded;
  uint8_t da1_responded;
  uint8_t da2_responded;
  uint8_t _pad2;
} zr_terminal_profile_t;

typedef struct zr_terminal_caps_t {
  plat_color_mode_t color_mode;
  uint8_t supports_mouse;
  uint8_t supports_bracketed_paste;
  uint8_t supports_focus_events;
  uint8_t supports_osc52;
  uint8_t supports_sync_update;
  uint8_t supports_scroll_region;
  uint8_t supports_cursor_shape;
  uint8_t supports_output_wait_writable;
  uint8_t supports_underline_styles;
  uint8_t supports_colored_underlines;
  uint8_t supports_hyperlinks;

  uint32_t sgr_attrs_supported;

  zr_terminal_id_t terminal_id;
  uint8_t _pad1[3];

  zr_terminal_cap_flags_t cap_flags;
  zr_terminal_cap_flags_t cap_force_flags;
  zr_terminal_cap_flags_t cap_suppress_flags;
} zr_terminal_caps_t;

#endif /* ZR_ZR_TERMINAL_CAPS_H_INCLUDED */
