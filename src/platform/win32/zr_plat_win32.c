/*
  src/platform/win32/zr_plat_win32.c — Win32 platform backend (console modes + VT + wake event).

  Why: Implements the OS-facing platform boundary on Windows:
    - VT output enable (required) and VT input enable (required, v1)
    - raw-mode enter/leave VT sequences (byte-for-byte locked ordering)
    - wakeable wait (STDIN handle + backend-owned wake event)
    - monotonic time via QPC
*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "platform/zr_platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* --- VT sequences (locked) --- */
static const uint8_t ZR_WIN32_SEQ_ALT_SCREEN_ENTER[] = "\x1b[?1049h";
static const uint8_t ZR_WIN32_SEQ_ALT_SCREEN_LEAVE[] = "\x1b[?1049l";
static const uint8_t ZR_WIN32_SEQ_CURSOR_HIDE[] = "\x1b[?25l";
static const uint8_t ZR_WIN32_SEQ_CURSOR_SHOW[] = "\x1b[?25h";
static const uint8_t ZR_WIN32_SEQ_WRAP_ENABLE[] = "\x1b[?7h";
static const uint8_t ZR_WIN32_SEQ_BRACKETED_PASTE_ENABLE[] = "\x1b[?2004h";
static const uint8_t ZR_WIN32_SEQ_BRACKETED_PASTE_DISABLE[] = "\x1b[?2004l";
static const uint8_t ZR_WIN32_SEQ_MOUSE_ENABLE[] = "\x1b[?1000h\x1b[?1006h";
static const uint8_t ZR_WIN32_SEQ_MOUSE_DISABLE[] = "\x1b[?1006l\x1b[?1000l";

zr_result_t zr_plat_win32_create(plat_t** out_plat, const plat_config_t* cfg);

struct plat_t {
  HANDLE h_in;
  HANDLE h_out;
  HANDLE h_wake_event;

  DWORD in_mode_orig;
  DWORD out_mode_orig;

  plat_size_t last_size;

  plat_config_t cfg;
  plat_caps_t   caps;

  bool    modes_valid;
  bool    raw_active;
  uint8_t _pad[6];
};

static zr_result_t zr_win32_write_all(HANDLE h_out, const uint8_t* bytes, int32_t len) {
  if (len < 0) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (len == 0) {
    return ZR_OK;
  }
  if (!bytes) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  int32_t written = 0;
  while (written < len) {
    DWORD chunk = (DWORD)(len - written);
    DWORD did_write = 0;
    BOOL ok = WriteFile(h_out, bytes + (size_t)written, chunk, &did_write, NULL);
    if (!ok) {
      return ZR_ERR_PLATFORM;
    }
    if (did_write == 0u) {
      return ZR_ERR_PLATFORM;
    }
    if (did_write > (DWORD)(len - written)) {
      return ZR_ERR_PLATFORM;
    }
    written += (int32_t)did_write;
  }

  return ZR_OK;
}

static zr_result_t zr_win32_write_cstr(HANDLE h_out, const uint8_t* s, size_t n_with_nul) {
  if (!s || n_with_nul == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (n_with_nul > (size_t)INT32_MAX + 1u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_win32_write_all(h_out, s, (int32_t)(n_with_nul - 1u));
}

static zr_result_t zr_win32_restore_modes_best_effort(plat_t* plat) {
  if (!plat || !plat->modes_valid) {
    return ZR_OK;
  }
  (void)SetConsoleMode(plat->h_in, plat->in_mode_orig);
  (void)SetConsoleMode(plat->h_out, plat->out_mode_orig);
  return ZR_OK;
}

/* Enable VT output/input per locked v1 rules; restores saved modes on failure. */
static zr_result_t zr_win32_enable_vt_or_fail(plat_t* plat) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  /* --- Save original modes --- */
  DWORD in_mode = 0u;
  DWORD out_mode = 0u;
  if (!GetConsoleMode(plat->h_in, &in_mode) || !GetConsoleMode(plat->h_out, &out_mode)) {
    return ZR_ERR_PLATFORM;
  }
  plat->in_mode_orig = in_mode;
  plat->out_mode_orig = out_mode;
  plat->modes_valid = true;

  /* --- Enable VT output (required) --- */
  DWORD out_mode_new = out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(plat->h_out, out_mode_new)) {
    (void)zr_win32_restore_modes_best_effort(plat);
    return ZR_ERR_PLATFORM;
  }
  DWORD out_mode_after = 0u;
  if (!GetConsoleMode(plat->h_out, &out_mode_after) || (out_mode_after & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0u) {
    (void)zr_win32_restore_modes_best_effort(plat);
    return ZR_ERR_PLATFORM;
  }

  /* --- Enable VT input (required; no legacy fallback in v1) --- */
  DWORD in_mode_new = in_mode | ENABLE_VIRTUAL_TERMINAL_INPUT;
  if (!SetConsoleMode(plat->h_in, in_mode_new)) {
    (void)zr_win32_restore_modes_best_effort(plat);
    return ZR_ERR_UNSUPPORTED;
  }
  DWORD in_mode_after = 0u;
  if (!GetConsoleMode(plat->h_in, &in_mode_after) || (in_mode_after & ENABLE_VIRTUAL_TERMINAL_INPUT) == 0u) {
    (void)zr_win32_restore_modes_best_effort(plat);
    return ZR_ERR_UNSUPPORTED;
  }

  return ZR_OK;
}

static void zr_win32_emit_enter_sequences_best_effort(plat_t* plat) {
  /*
    Locked ordering for enter:
      ?1049h, ?25l, ?7h, ?2004h, ?1000h?1006h (when enabled by config/caps)
  */
  (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_ALT_SCREEN_ENTER, sizeof(ZR_WIN32_SEQ_ALT_SCREEN_ENTER));
  (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_CURSOR_HIDE, sizeof(ZR_WIN32_SEQ_CURSOR_HIDE));
  (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_WRAP_ENABLE, sizeof(ZR_WIN32_SEQ_WRAP_ENABLE));

  if (plat->cfg.enable_bracketed_paste && plat->caps.supports_bracketed_paste) {
    (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_BRACKETED_PASTE_ENABLE, sizeof(ZR_WIN32_SEQ_BRACKETED_PASTE_ENABLE));
  }
  if (plat->cfg.enable_mouse && plat->caps.supports_mouse) {
    (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_MOUSE_ENABLE, sizeof(ZR_WIN32_SEQ_MOUSE_ENABLE));
  }
}

static void zr_win32_emit_leave_sequences_best_effort(plat_t* plat) {
  /*
    Best-effort restore on leave:
      - disable mouse / bracketed paste
      - show cursor
      - leave alt screen
      - wrap policy: leave wrap enabled
  */
  if (plat->cfg.enable_mouse && plat->caps.supports_mouse) {
    (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_MOUSE_DISABLE, sizeof(ZR_WIN32_SEQ_MOUSE_DISABLE));
  }
  if (plat->cfg.enable_bracketed_paste && plat->caps.supports_bracketed_paste) {
    (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_BRACKETED_PASTE_DISABLE, sizeof(ZR_WIN32_SEQ_BRACKETED_PASTE_DISABLE));
  }

  (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_WRAP_ENABLE, sizeof(ZR_WIN32_SEQ_WRAP_ENABLE));
  (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_CURSOR_SHOW, sizeof(ZR_WIN32_SEQ_CURSOR_SHOW));
  (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_ALT_SCREEN_LEAVE, sizeof(ZR_WIN32_SEQ_ALT_SCREEN_LEAVE));
}

static bool zr_win32_query_size_best_effort(HANDLE h_out, plat_size_t* inout_last_size) {
  if (!inout_last_size) {
    return false;
  }

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  memset(&csbi, 0, sizeof(csbi));
  if (!GetConsoleScreenBufferInfo(h_out, &csbi)) {
    return false;
  }

  int cols_i = (int)csbi.srWindow.Right - (int)csbi.srWindow.Left + 1;
  int rows_i = (int)csbi.srWindow.Bottom - (int)csbi.srWindow.Top + 1;
  if (cols_i <= 0 || rows_i <= 0) {
    return false;
  }

  inout_last_size->cols = (uint32_t)cols_i;
  inout_last_size->rows = (uint32_t)rows_i;
  return true;
}

/* Create Win32 platform handle with wake event and conservative default caps. */
zr_result_t zr_plat_win32_create(plat_t** out_plat, const plat_config_t* cfg) {
  if (!out_plat || !cfg) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_plat = NULL;

  plat_t* plat = (plat_t*)calloc(1u, sizeof(*plat));
  if (!plat) {
    return ZR_ERR_OOM;
  }

  plat->cfg = *cfg;
  plat->h_in = GetStdHandle(STD_INPUT_HANDLE);
  plat->h_out = GetStdHandle(STD_OUTPUT_HANDLE);
  plat->in_mode_orig = 0u;
  plat->out_mode_orig = 0u;
  plat->modes_valid = false;
  plat->raw_active = false;
  plat->last_size.cols = 0u;
  plat->last_size.rows = 0u;

  if (!plat->h_in || plat->h_in == INVALID_HANDLE_VALUE || !plat->h_out || plat->h_out == INVALID_HANDLE_VALUE) {
    free(plat);
    return ZR_ERR_PLATFORM;
  }

  plat->h_wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
  if (!plat->h_wake_event) {
    free(plat);
    return ZR_ERR_PLATFORM;
  }

  /*
    v1 caps are conservative and deterministic: if the environment supports VT
    output/input (required on enter), these sequences are safe to emit.
  */
  plat->caps.color_mode = cfg->requested_color_mode;
  plat->caps.supports_mouse = 1u;
  plat->caps.supports_bracketed_paste = 1u;
  plat->caps.supports_focus_events = 1u;
  plat->caps.supports_osc52 = 1u;
  plat->caps._pad[0] = 0u;
  plat->caps._pad[1] = 0u;
  plat->caps._pad[2] = 0u;
  plat->caps.sgr_attrs_supported = 0xFFFFFFFFu;

  (void)zr_win32_query_size_best_effort(plat->h_out, &plat->last_size);

  *out_plat = plat;
  return ZR_OK;
}

void plat_destroy(plat_t* plat) {
  if (!plat) {
    return;
  }

  (void)plat_leave_raw(plat);

  if (plat->h_wake_event) {
    (void)CloseHandle(plat->h_wake_event);
    plat->h_wake_event = NULL;
  }

  free(plat);
}

/* Enter raw mode: enable VT I/O (locked v1) and emit deterministic enter sequences. */
zr_result_t plat_enter_raw(plat_t* plat) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (plat->raw_active) {
    return ZR_OK;
  }

  zr_result_t r = zr_win32_enable_vt_or_fail(plat);
  if (r != ZR_OK) {
    return r;
  }

  zr_win32_emit_enter_sequences_best_effort(plat);
  plat->raw_active = true;
  return ZR_OK;
}

/* Leave raw mode: restore saved console modes and emit leave sequences. Idempotent. */
zr_result_t plat_leave_raw(plat_t* plat) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  /*
    Idempotent + best-effort:
      - Attempt to restore the terminal even if we were never marked active.
      - Never block indefinitely.
  */
  if (plat->raw_active) {
    zr_win32_emit_leave_sequences_best_effort(plat);
  }

  (void)zr_win32_restore_modes_best_effort(plat);

  plat->raw_active = false;
  return ZR_OK;
}

zr_result_t plat_get_size(plat_t* plat, plat_size_t* out_size) {
  if (!plat || !out_size) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  (void)zr_win32_query_size_best_effort(plat->h_out, &plat->last_size);
  *out_size = plat->last_size;
  return ZR_OK;
}

zr_result_t plat_get_caps(plat_t* plat, plat_caps_t* out_caps) {
  if (!plat || !out_caps) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_caps = plat->caps;
  return ZR_OK;
}

/* Non-blocking read from console input; returns bytes read, 0 if none available, or error. */
int32_t plat_read_input(plat_t* plat, uint8_t* out_buf, int32_t out_cap) {
  if (!plat) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }
  if (out_cap < 0) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }
  if (out_cap == 0) {
    return 0;
  }
  if (!out_buf) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }

  DWORD wait_rc = WaitForSingleObject(plat->h_in, 0u);
  if (wait_rc == WAIT_TIMEOUT) {
    return 0;
  }
  if (wait_rc != WAIT_OBJECT_0) {
    return (int32_t)ZR_ERR_PLATFORM;
  }

  DWORD n = 0u;
  BOOL ok = ReadFile(plat->h_in, out_buf, (DWORD)out_cap, &n, NULL);
  if (!ok) {
    return (int32_t)ZR_ERR_PLATFORM;
  }
  if (n > (DWORD)INT32_MAX) {
    return (int32_t)ZR_ERR_PLATFORM;
  }
  return (int32_t)n;
}

zr_result_t plat_write_output(plat_t* plat, const uint8_t* bytes, int32_t len) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_win32_write_all(plat->h_out, bytes, len);
}

/* Wait for input or wake event; returns 1 if ready, 0 on timeout, or error code. */
int32_t plat_wait(plat_t* plat, int32_t timeout_ms) {
  if (!plat) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }

  DWORD timeout = INFINITE;
  if (timeout_ms >= 0) {
    timeout = (DWORD)timeout_ms;
  }

  HANDLE handles[2];
  handles[0] = plat->h_in;
  handles[1] = plat->h_wake_event;

  DWORD rc = WaitForMultipleObjects(2u, handles, FALSE, timeout);
  if (rc == WAIT_TIMEOUT) {
    return 0;
  }
  if (rc == WAIT_OBJECT_0 || rc == (WAIT_OBJECT_0 + 1u)) {
    return 1;
  }

  return (int32_t)ZR_ERR_PLATFORM;
}

/* Wake a blocked plat_wait call from another thread by signaling the wake event. */
zr_result_t plat_wake(plat_t* plat) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!plat->h_wake_event) {
    return ZR_ERR_PLATFORM;
  }
  if (!SetEvent(plat->h_wake_event)) {
    return ZR_ERR_PLATFORM;
  }
  return ZR_OK;
}

uint64_t plat_now_ms(void) {
  LARGE_INTEGER freq;
  LARGE_INTEGER now;
  if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0) {
    return 0ull;
  }
  if (!QueryPerformanceCounter(&now)) {
    return 0ull;
  }

  uint64_t ticks = (uint64_t)now.QuadPart;
  uint64_t hz = (uint64_t)freq.QuadPart;
  return (ticks * 1000ull) / hz;
}
