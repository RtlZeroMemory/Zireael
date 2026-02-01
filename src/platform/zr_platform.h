/*
  src/platform/zr_platform.h — OS-header-free platform boundary interface.

  Why: Provides a hard boundary between the deterministic core (no OS headers)
  and platform backends (POSIX/Win32). Core code talks only to this interface.
*/

#ifndef ZR_PLATFORM_ZR_PLATFORM_H_INCLUDED
#define ZR_PLATFORM_ZR_PLATFORM_H_INCLUDED

#include "util/zr_result.h"

#include <stdint.h>

/* Opaque platform handle. Implemented by the platform backend. */
typedef struct plat_t plat_t;

/*
  plat_color_mode_t:
    - A fixed-width, ABI-stable color capability / request.
*/
typedef uint8_t plat_color_mode_t;
#define PLAT_COLOR_MODE_UNKNOWN ((plat_color_mode_t)0u)
#define PLAT_COLOR_MODE_16      ((plat_color_mode_t)1u)
#define PLAT_COLOR_MODE_256     ((plat_color_mode_t)2u)
#define PLAT_COLOR_MODE_RGB     ((plat_color_mode_t)3u)

/*
  plat_size_t:
    - Terminal size in character cells.
*/
typedef struct plat_size_t {
  uint32_t cols;
  uint32_t rows;
} plat_size_t;

/*
  plat_caps_t:
    - Backend-discovered capabilities.
    - Boolean-like fields are encoded as 0/1 bytes for ABI stability.
*/
typedef struct plat_caps_t {
  plat_color_mode_t color_mode;
  uint8_t           supports_mouse;
  uint8_t           supports_bracketed_paste;
  uint8_t           supports_focus_events;
  uint8_t           supports_osc52;
  uint8_t           _pad[3];

  /*
    sgr_attrs_supported:
      - Bitmask of supported zr_style_t attrs for SGR emission.
      - Diff renderer must AND desired attrs with this mask deterministically.
  */
  uint32_t          sgr_attrs_supported;
} plat_caps_t;

/*
  plat_config_t:
    - Core-provided desired platform behavior.
*/
typedef struct plat_config_t {
  plat_color_mode_t requested_color_mode;
  uint8_t           enable_mouse;
  uint8_t           enable_bracketed_paste;
  uint8_t           enable_focus_events;
  uint8_t           enable_osc52;
  uint8_t           _pad[3];
} plat_config_t;

/* lifecycle */
zr_result_t plat_create(plat_t** out_plat, const plat_config_t* cfg);
void        plat_destroy(plat_t* plat);

/* raw mode (idempotent, best-effort) */
zr_result_t plat_enter_raw(plat_t* plat);
zr_result_t plat_leave_raw(plat_t* plat);

/* caps/size */
zr_result_t plat_get_size(plat_t* plat, plat_size_t* out_size);
zr_result_t plat_get_caps(plat_t* plat, plat_caps_t* out_caps);

/* I/O */
int32_t     plat_read_input(plat_t* plat, uint8_t* out_buf, int32_t out_cap);
zr_result_t plat_write_output(plat_t* plat, const uint8_t* bytes, int32_t len);

/*
  wait/wake:
    - plat_wait returns:
        1 : woke or input-ready
        0 : timeout
      < 0 : negative ZR_ERR_* failure
    - plat_wake is callable from non-engine threads and must not block indefinitely.
*/
int32_t     plat_wait(plat_t* plat, int32_t timeout_ms);
zr_result_t plat_wake(plat_t* plat);

/* time */
uint64_t plat_now_ms(void);

#endif /* ZR_PLATFORM_ZR_PLATFORM_H_INCLUDED */
