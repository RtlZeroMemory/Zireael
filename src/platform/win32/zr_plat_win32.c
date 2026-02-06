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
static const uint32_t ZR_WIN32_UTF16_HIGH_SURROGATE_MIN = 0xD800u;
static const uint32_t ZR_WIN32_UTF16_HIGH_SURROGATE_MAX = 0xDBFFu;
static const uint32_t ZR_WIN32_UTF16_LOW_SURROGATE_MIN = 0xDC00u;
static const uint32_t ZR_WIN32_UTF16_LOW_SURROGATE_MAX = 0xDFFFu;

zr_result_t zr_plat_win32_create(plat_t** out_plat, const plat_config_t* cfg);

struct plat_t {
  HANDLE h_in;
  HANDLE h_out;
  HANDLE h_wake_event;

  DWORD in_mode_orig;
  DWORD out_mode_orig;

  UINT in_cp_orig;
  UINT out_cp_orig;

  plat_size_t last_size;

  plat_config_t cfg;
  plat_caps_t caps;

  bool modes_valid;
  bool cp_valid;
  bool raw_active;
  bool has_pending_high_surrogate;
  uint16_t pending_high_surrogate;
  uint8_t _pad[2];
};

static void zr_win32_emit_repeat(uint8_t* out_buf, size_t out_cap, size_t* io_len, const uint8_t* seq, size_t seq_len,
                                 WORD repeat) {
  if (!out_buf || !io_len || !seq) {
    return;
  }
  if (seq_len == 0u) {
    return;
  }
  if (repeat == 0u) {
    repeat = 1u;
  }
  for (WORD r = 0u; r < repeat; r++) {
    if (*io_len + seq_len > out_cap) {
      return;
    }
    memcpy(out_buf + *io_len, seq, seq_len);
    *io_len += seq_len;
  }
}

static size_t zr_win32_encode_utf8_scalar(uint32_t scalar, uint8_t out[4]) {
  if (!out) {
    return 0u;
  }

  if (scalar > 0x10FFFFu || (scalar >= 0xD800u && scalar <= 0xDFFFu)) {
    scalar = 0xFFFDu;
  }

  if (scalar <= 0x7Fu) {
    out[0] = (uint8_t)scalar;
    return 1u;
  }
  if (scalar <= 0x7FFu) {
    out[0] = (uint8_t)(0xC0u | ((scalar >> 6u) & 0x1Fu));
    out[1] = (uint8_t)(0x80u | (scalar & 0x3Fu));
    return 2u;
  }
  if (scalar <= 0xFFFFu) {
    out[0] = (uint8_t)(0xE0u | ((scalar >> 12u) & 0x0Fu));
    out[1] = (uint8_t)(0x80u | ((scalar >> 6u) & 0x3Fu));
    out[2] = (uint8_t)(0x80u | (scalar & 0x3Fu));
    return 3u;
  }

  out[0] = (uint8_t)(0xF0u | ((scalar >> 18u) & 0x07u));
  out[1] = (uint8_t)(0x80u | ((scalar >> 12u) & 0x3Fu));
  out[2] = (uint8_t)(0x80u | ((scalar >> 6u) & 0x3Fu));
  out[3] = (uint8_t)(0x80u | (scalar & 0x3Fu));
  return 4u;
}

static bool zr_win32_is_high_surrogate(uint32_t scalar) {
  return scalar >= ZR_WIN32_UTF16_HIGH_SURROGATE_MIN && scalar <= ZR_WIN32_UTF16_HIGH_SURROGATE_MAX;
}

static bool zr_win32_is_low_surrogate(uint32_t scalar) {
  return scalar >= ZR_WIN32_UTF16_LOW_SURROGATE_MIN && scalar <= ZR_WIN32_UTF16_LOW_SURROGATE_MAX;
}

static uint32_t zr_win32_decode_surrogate_pair(uint32_t high, uint32_t low) {
  const uint32_t hi10 = high - ZR_WIN32_UTF16_HIGH_SURROGATE_MIN;
  const uint32_t lo10 = low - ZR_WIN32_UTF16_LOW_SURROGATE_MIN;
  return 0x10000u + (hi10 << 10u) + lo10;
}

static void zr_win32_emit_utf8_scalar_repeat(uint8_t* out_buf, size_t out_cap, size_t* io_len, uint32_t scalar,
                                             WORD repeat) {
  uint8_t utf8[4];
  const size_t n = zr_win32_encode_utf8_scalar(scalar, utf8);
  zr_win32_emit_repeat(out_buf, out_cap, io_len, utf8, n, repeat);
}

static void zr_win32_flush_pending_high_surrogate(plat_t* plat, uint8_t* out_buf, size_t out_cap, size_t* io_len) {
  if (!plat || !plat->has_pending_high_surrogate) {
    return;
  }
  zr_win32_emit_utf8_scalar_repeat(out_buf, out_cap, io_len, 0xFFFDu, 1u);
  plat->has_pending_high_surrogate = false;
  plat->pending_high_surrogate = 0u;
}

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
  if (plat->cp_valid) {
    (void)SetConsoleCP(plat->in_cp_orig);
    (void)SetConsoleOutputCP(plat->out_cp_orig);
  }
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

  /* --- Prefer UTF-8 console code pages for correct glyph rendering --- */
  plat->in_cp_orig = GetConsoleCP();
  plat->out_cp_orig = GetConsoleOutputCP();
  plat->cp_valid = (plat->in_cp_orig != 0u && plat->out_cp_orig != 0u);
  if (!SetConsoleCP(CP_UTF8) || !SetConsoleOutputCP(CP_UTF8)) {
    (void)zr_win32_restore_modes_best_effort(plat);
    return ZR_ERR_UNSUPPORTED;
  }
  if (GetConsoleCP() != (UINT)CP_UTF8 || GetConsoleOutputCP() != (UINT)CP_UTF8) {
    (void)zr_win32_restore_modes_best_effort(plat);
    return ZR_ERR_UNSUPPORTED;
  }

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
  /*
    "Raw" input in practice means:
      - no cooked line buffering and no echo
      - no ctrl-c signal translation (engine parses bytes)
      - avoid QuickEdit mode (can freeze input on mouse selection)

    Note: we intentionally keep this logic deterministic and config-driven.
  */
  DWORD in_mode_base = in_mode | ENABLE_VIRTUAL_TERMINAL_INPUT;

  /*
    QuickEdit handling is best-effort: only toggle the bit when the console host
    already exposes EXTENDED_FLAGS behavior.
  */
  if ((in_mode & ENABLE_EXTENDED_FLAGS) != 0u) {
    in_mode_base &= ~((DWORD)ENABLE_QUICK_EDIT_MODE);
  }

  /*
    Some environments (notably certain ConPTY configurations) reject aggressive
    mode bit clearing. Try a strict raw-ish mode first; fall back to a minimal,
    VT-input-capable mode on failure. The fallback ladder must still disable
    line buffering; otherwise, input may not be delivered until Enter.
  */
  DWORD candidates[4];
  candidates[0] =
      in_mode_base & ~((DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT));
  candidates[1] = in_mode_base & ~((DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_WINDOW_INPUT));
  candidates[2] = in_mode_base & ~((DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
  candidates[3] = in_mode_base;

  bool set_ok = false;
  for (size_t i = 0u; i < (sizeof(candidates) / sizeof(candidates[0])); i++) {
    if (SetConsoleMode(plat->h_in, candidates[i])) {
      set_ok = true;
      break;
    }
  }
  if (!set_ok) {
    (void)zr_win32_restore_modes_best_effort(plat);
    return ZR_ERR_UNSUPPORTED;
  }
  DWORD in_mode_after = 0u;
  if (!GetConsoleMode(plat->h_in, &in_mode_after) || (in_mode_after & ENABLE_VIRTUAL_TERMINAL_INPUT) == 0u) {
    (void)zr_win32_restore_modes_best_effort(plat);
    return ZR_ERR_UNSUPPORTED;
  }
  if ((in_mode_after & ENABLE_LINE_INPUT) != 0u) {
    /*
      Without disabling line input, ReadFile() may block until Enter and arrow
      keys won't arrive as VT sequences. Treat as unsupported so callers can
      surface a clear error.
    */
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
    (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_BRACKETED_PASTE_ENABLE,
                              sizeof(ZR_WIN32_SEQ_BRACKETED_PASTE_ENABLE));
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
    (void)zr_win32_write_cstr(plat->h_out, ZR_WIN32_SEQ_BRACKETED_PASTE_DISABLE,
                              sizeof(ZR_WIN32_SEQ_BRACKETED_PASTE_DISABLE));
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
  plat->has_pending_high_surrogate = false;
  plat->pending_high_surrogate = 0u;
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
  /* Focus in/out bytes are not normalized by the core parser in v1. */
  plat->caps.supports_focus_events = 0u;
  plat->caps.supports_osc52 = 1u;
  plat->caps.supports_sync_update = 1u;
  plat->caps.supports_scroll_region = 1u;
  plat->caps.supports_cursor_shape = 1u;
  plat->caps.supports_output_wait_writable = 0u;
  plat->caps._pad0[0] = 0u;
  plat->caps._pad0[1] = 0u;
  plat->caps._pad0[2] = 0u;
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

  if (!zr_win32_query_size_best_effort(plat->h_out, &plat->last_size)) {
    out_size->cols = 0u;
    out_size->rows = 0u;
    return ZR_ERR_PLATFORM;
  }

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

  /*
    Non-blocking read is subtle on Windows:
      - ConPTY and some hosts present STDIN as a pipe; waitable handles may still
        appear signaled when no bytes are currently readable.
      - Console input handles may not behave like pipes for readiness queries.

    Rule: never call ReadFile unless we have strong evidence that it will not
    block. Prefer explicit "bytes available" probes when possible.
  */
  const DWORD ft = GetFileType(plat->h_in);
  if (ft == FILE_TYPE_PIPE) {
    DWORD avail = 0u;
    BOOL ok = PeekNamedPipe(plat->h_in, NULL, 0u, NULL, &avail, NULL);
    if (!ok) {
      return (int32_t)ZR_ERR_PLATFORM;
    }
    if (avail == 0u) {
      return 0;
    }
  } else if (ft == FILE_TYPE_CHAR) {
    /*
      Avoid ReadFile() on console input handles: it can still block when the
      handle is signaled due to non-key input records (mouse/focus/resize).

      Instead, consume INPUT_RECORDs and translate only key-down events into
      the minimal byte stream our core parser understands.
    */
    DWORD n_events = 0u;
    if (!GetNumberOfConsoleInputEvents(plat->h_in, &n_events)) {
      return (int32_t)ZR_ERR_PLATFORM;
    }
    if (n_events == 0u) {
      return 0;
    }

    INPUT_RECORD recs[32];
    DWORD read = 0u;
    DWORD want = n_events;
    if (want > (DWORD)(sizeof(recs) / sizeof(recs[0]))) {
      want = (DWORD)(sizeof(recs) / sizeof(recs[0]));
    }
    if (!ReadConsoleInputW(plat->h_in, recs, want, &read)) {
      return (int32_t)ZR_ERR_PLATFORM;
    }

    size_t out_len = 0u;
    const size_t out_cap_z = (size_t)out_cap;
    for (DWORD i = 0u; i < read; i++) {
      const INPUT_RECORD* r = &recs[i];
      if (r->EventType != KEY_EVENT) {
        continue;
      }

      const KEY_EVENT_RECORD* k = &r->Event.KeyEvent;
      if (!k->bKeyDown) {
        continue;
      }

      const WORD vk = k->wVirtualKeyCode;
      const WCHAR ch = k->uChar.UnicodeChar;
      const WORD repeat = k->wRepeatCount;

      if (vk == VK_UP) {
        zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
        const uint8_t seq[] = {0x1Bu, (uint8_t)'[', (uint8_t)'A'};
        zr_win32_emit_repeat(out_buf, out_cap_z, &out_len, seq, sizeof(seq), repeat);
        continue;
      }
      if (vk == VK_DOWN) {
        zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
        const uint8_t seq[] = {0x1Bu, (uint8_t)'[', (uint8_t)'B'};
        zr_win32_emit_repeat(out_buf, out_cap_z, &out_len, seq, sizeof(seq), repeat);
        continue;
      }
      if (vk == VK_RIGHT) {
        zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
        const uint8_t seq[] = {0x1Bu, (uint8_t)'[', (uint8_t)'C'};
        zr_win32_emit_repeat(out_buf, out_cap_z, &out_len, seq, sizeof(seq), repeat);
        continue;
      }
      if (vk == VK_LEFT) {
        zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
        const uint8_t seq[] = {0x1Bu, (uint8_t)'[', (uint8_t)'D'};
        zr_win32_emit_repeat(out_buf, out_cap_z, &out_len, seq, sizeof(seq), repeat);
        continue;
      }
      if (vk == VK_RETURN) {
        zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
        const uint8_t seq[] = {(uint8_t)'\r'};
        zr_win32_emit_repeat(out_buf, out_cap_z, &out_len, seq, sizeof(seq), repeat);
        continue;
      }
      if (vk == VK_ESCAPE) {
        zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
        const uint8_t seq[] = {0x1Bu};
        zr_win32_emit_repeat(out_buf, out_cap_z, &out_len, seq, sizeof(seq), repeat);
        continue;
      }
      if (vk == VK_TAB) {
        zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
        const uint8_t seq[] = {(uint8_t)'\t'};
        zr_win32_emit_repeat(out_buf, out_cap_z, &out_len, seq, sizeof(seq), repeat);
        continue;
      }
      if (vk == VK_BACK) {
        zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
        const uint8_t seq[] = {0x7Fu};
        zr_win32_emit_repeat(out_buf, out_cap_z, &out_len, seq, sizeof(seq), repeat);
        continue;
      }

      if (ch == 0) {
        zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
        continue;
      }

      if (zr_win32_is_high_surrogate((uint32_t)ch)) {
        zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
        plat->has_pending_high_surrogate = true;
        plat->pending_high_surrogate = (uint16_t)ch;
        continue;
      }

      if (zr_win32_is_low_surrogate((uint32_t)ch)) {
        if (plat->has_pending_high_surrogate) {
          const uint32_t scalar = zr_win32_decode_surrogate_pair((uint32_t)plat->pending_high_surrogate, (uint32_t)ch);
          plat->has_pending_high_surrogate = false;
          plat->pending_high_surrogate = 0u;
          zr_win32_emit_utf8_scalar_repeat(out_buf, out_cap_z, &out_len, scalar, repeat);
          continue;
        }
        zr_win32_emit_utf8_scalar_repeat(out_buf, out_cap_z, &out_len, 0xFFFDu, repeat);
        continue;
      }

      zr_win32_flush_pending_high_surrogate(plat, out_buf, out_cap_z, &out_len);
      zr_win32_emit_utf8_scalar_repeat(out_buf, out_cap_z, &out_len, (uint32_t)ch, repeat);
    }

    if (out_len > (size_t)INT32_MAX) {
      return (int32_t)ZR_ERR_PLATFORM;
    }
    return (int32_t)out_len;
  } else {
    DWORD wait_rc = WaitForSingleObject(plat->h_in, 0u);
    if (wait_rc == WAIT_TIMEOUT) {
      return 0;
    }
    if (wait_rc != WAIT_OBJECT_0) {
      return (int32_t)ZR_ERR_PLATFORM;
    }
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

zr_result_t plat_wait_output_writable(plat_t* plat, int32_t timeout_ms) {
  (void)plat;
  (void)timeout_ms;
  return ZR_ERR_UNSUPPORTED;
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

  uint64_t seconds = ticks / hz;
  uint64_t rem = ticks % hz;
  if (seconds > UINT64_MAX / 1000ull) {
    return UINT64_MAX;
  }
  return (seconds * 1000ull) + ((rem * 1000ull) / hz);
}
