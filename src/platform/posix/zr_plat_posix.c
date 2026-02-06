/*
  src/platform/posix/zr_plat_posix.c — POSIX platform backend (termios + self-pipe wake).

  Why: Implements the OS-facing platform boundary for POSIX terminals:
    - raw mode enter/leave (idempotent, best-effort restore on leave)
    - non-blocking input reads
    - poll()-based wait that can be interrupted by a self-pipe wake (threads + signals)
*/

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif

#if !defined(_POSIX_C_SOURCE) && !defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "platform/zr_platform.h"

#include "util/zr_assert.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

struct plat_t {
  plat_config_t cfg;
  plat_caps_t caps;

  int stdin_fd;
  int stdout_fd;
  int tty_fd_owned;

  int wake_read_fd;
  int wake_write_fd;

  int stdin_flags_saved;
  bool stdin_flags_valid;
  struct termios termios_saved;
  bool termios_valid;

  bool raw_active;

  struct sigaction sigwinch_prev;
  bool sigwinch_installed;
};

static volatile sig_atomic_t g_posix_sigwinch_pending = 0;
static int g_posix_wake_write_fd = -1;

static const char* zr_posix_getenv_nonempty(const char* key) {
  if (!key) {
    return NULL;
  }
  const char* v = getenv(key);
  if (!v || v[0] == '\0') {
    return NULL;
  }
  return v;
}

static bool zr_posix_term_is_dumb(void) {
  const char* term = zr_posix_getenv_nonempty("TERM");
  if (!term) {
    return true;
  }
  return strcmp(term, "dumb") == 0;
}

static uint8_t zr_posix_detect_scroll_region(void) {
  /*
    Scroll regions (DECSTBM + SU/SD) are part of the common VT/xterm feature
    set. Avoid emitting them only for "dumb" terminals (where any VT output is
    likely ineffective).
  */
  return zr_posix_term_is_dumb() ? 0u : 1u;
}

static uint8_t zr_posix_detect_sync_update(void) {
  /*
    Synchronized output (DEC private mode ?2026) is not universally supported.
    Use a conservative allowlist based on well-known environment markers.
  */
  if (zr_posix_term_is_dumb()) {
    return 0u;
  }

  if (zr_posix_getenv_nonempty("KITTY_WINDOW_ID")) {
    return 1u;
  }
  if (zr_posix_getenv_nonempty("WEZTERM_PANE") || zr_posix_getenv_nonempty("WEZTERM_EXECUTABLE")) {
    return 1u;
  }

  const char* term_program = zr_posix_getenv_nonempty("TERM_PROGRAM");
  if (term_program && strcmp(term_program, "iTerm.app") == 0) {
    return 1u;
  }
  if (term_program && (strcmp(term_program, "Rio") == 0 || strcmp(term_program, "rio") == 0)) {
    return 1u;
  }

  const char* term = zr_posix_getenv_nonempty("TERM");
  if (term && (strstr(term, "kitty") || strstr(term, "wezterm") || strstr(term, "rio"))) {
    return 1u;
  }

  return 0u;
}

static void zr_posix_sigwinch_handler(int signo) {
  (void)signo;
  g_posix_sigwinch_pending = 1;

  int saved_errno = errno;
  if (g_posix_wake_write_fd >= 0) {
    const uint8_t b = 0u;
    (void)write(g_posix_wake_write_fd, &b, 1u);
  }
  errno = saved_errno;
}

static zr_result_t zr_posix_set_fd_flag(int fd, int flag, bool enabled) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return ZR_ERR_PLATFORM;
  }
  int desired = enabled ? (flags | flag) : (flags & ~flag);
  if (desired == flags) {
    return ZR_OK;
  }
  if (fcntl(fd, F_SETFL, desired) != 0) {
    return ZR_ERR_PLATFORM;
  }
  return ZR_OK;
}

static zr_result_t zr_posix_set_fd_cloexec(int fd) {
  int flags = fcntl(fd, F_GETFD, 0);
  if (flags < 0) {
    return ZR_ERR_PLATFORM;
  }
  if ((flags & FD_CLOEXEC) != 0) {
    return ZR_OK;
  }
  if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) != 0) {
    return ZR_ERR_PLATFORM;
  }
  return ZR_OK;
}

/* Create a non-blocking self-pipe pair for cross-thread wake signaling. */
static zr_result_t zr_posix_make_self_pipe(int* out_read_fd, int* out_write_fd) {
  if (!out_read_fd || !out_write_fd) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_read_fd = -1;
  *out_write_fd = -1;

  int fds[2] = {-1, -1};
  if (pipe(fds) != 0) {
    return ZR_ERR_PLATFORM;
  }
  if (zr_posix_set_fd_cloexec(fds[0]) != ZR_OK || zr_posix_set_fd_cloexec(fds[1]) != ZR_OK) {
    (void)close(fds[0]);
    (void)close(fds[1]);
    return ZR_ERR_PLATFORM;
  }
  if (zr_posix_set_fd_flag(fds[0], O_NONBLOCK, true) != ZR_OK ||
      zr_posix_set_fd_flag(fds[1], O_NONBLOCK, true) != ZR_OK) {
    (void)close(fds[0]);
    (void)close(fds[1]);
    return ZR_ERR_PLATFORM;
  }

  *out_read_fd = fds[0];
  *out_write_fd = fds[1];
  return ZR_OK;
}

static void zr_posix_drain_fd_best_effort(int fd) {
  uint8_t buf[256];
  for (;;) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
      continue;
    }
    if (n == 0) {
      return;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    return;
  }
}

static zr_result_t zr_posix_wait_writable(int fd) {
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLOUT;
  pfd.revents = 0;

  for (;;) {
    int rc = poll(&pfd, 1u, -1);
    if (rc > 0) {
      if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        return ZR_ERR_PLATFORM;
      }
      return ZR_OK;
    }
    if (rc == 0) {
      continue;
    }
    if (errno == EINTR) {
      continue;
    }
    return ZR_ERR_PLATFORM;
  }
}

static uint64_t zr_posix_now_ms_monotonic_best_effort(void) {
  struct timespec ts;
  memset(&ts, 0, sizeof(ts));
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0u;
  }
  const uint64_t s = (uint64_t)ts.tv_sec;
  const uint64_t ns = (uint64_t)ts.tv_nsec;
  return (s * 1000u) + (ns / 1000000u);
}

static zr_result_t zr_posix_wait_writable_timeout(int fd, int32_t timeout_ms) {
  if (timeout_ms < 0) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLOUT;
  pfd.revents = 0;

  const uint64_t start_ms = zr_posix_now_ms_monotonic_best_effort();
  int32_t remaining = timeout_ms;

  for (;;) {
    int rc = poll(&pfd, 1u, remaining);
    if (rc > 0) {
      if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        return ZR_ERR_PLATFORM;
      }
      return ((pfd.revents & POLLOUT) != 0) ? ZR_OK : ZR_ERR_PLATFORM;
    }
    if (rc == 0) {
      return ZR_ERR_LIMIT;
    }
    if (errno == EINTR) {
      if (timeout_ms == 0) {
        return ZR_ERR_LIMIT;
      }
      const uint64_t now_ms = zr_posix_now_ms_monotonic_best_effort();
      if (start_ms != 0u && now_ms >= start_ms) {
        const uint64_t elapsed = now_ms - start_ms;
        if (elapsed >= (uint64_t)timeout_ms) {
          return ZR_ERR_LIMIT;
        }
        remaining = (int32_t)((uint64_t)timeout_ms - elapsed);
      }
      continue;
    }
    return ZR_ERR_PLATFORM;
  }
}

/* Write all bytes to fd, retrying on EINTR; returns error on partial write failure. */
static zr_result_t zr_posix_write_all(int fd, const uint8_t* bytes, int32_t len) {
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
    ssize_t n = write(fd, bytes + (size_t)written, (size_t)(len - written));
    if (n > 0) {
      written += (int32_t)n;
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      /*
        Terminals are typically blocking, but stdout may be configured as
        non-blocking by a parent process or wrapper. Treat EAGAIN as a
        transient backpressure signal and wait until the fd is writable.
      */
      zr_result_t rc = zr_posix_wait_writable(fd);
      if (rc != ZR_OK) {
        return rc;
      }
      continue;
    }
    if (n < 0 && errno == EINTR) {
      continue;
    }
    return ZR_ERR_PLATFORM;
  }

  return ZR_OK;
}

zr_result_t plat_wait_output_writable(plat_t* plat, int32_t timeout_ms) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_posix_wait_writable_timeout(plat->stdout_fd, timeout_ms);
}

static zr_result_t zr_posix_write_cstr(int fd, const char* s) {
  if (!s) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  size_t n = strlen(s);
  if (n > (size_t)INT32_MAX) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_posix_write_all(fd, (const uint8_t*)s, (int32_t)n);
}

static void zr_posix_emit_enter_sequences(plat_t* plat) {
  /*
    Locked ordering for enter:
      ?1049h, ?25l, ?7h, ?2004h, ?1000h?1002h?1003h?1006h (when enabled by config/caps)
  */
  (void)zr_posix_write_cstr(plat->stdout_fd, "\x1b[?1049h");
  (void)zr_posix_write_cstr(plat->stdout_fd, "\x1b[?25l");
  (void)zr_posix_write_cstr(plat->stdout_fd, "\x1b[?7h");

  if (plat->cfg.enable_bracketed_paste && plat->caps.supports_bracketed_paste) {
    (void)zr_posix_write_cstr(plat->stdout_fd, "\x1b[?2004h");
  }
  if (plat->cfg.enable_mouse && plat->caps.supports_mouse) {
    /*
      Mouse tracking:
        - ?1000h: report button press/release
        - ?1002h: report drag motion
        - ?1003h: report any motion (hover)
        - ?1006h: SGR encoding (needed for >223 coords and modern terminals)
    */
    (void)zr_posix_write_cstr(plat->stdout_fd, "\x1b[?1000h\x1b[?1002h\x1b[?1003h\x1b[?1006h");
  }
}

static void zr_posix_emit_leave_sequences(plat_t* plat) {
  /*
    Best-effort restore on leave:
      - disable mouse / bracketed paste
      - show cursor
      - leave alt screen
      - wrap policy: leave wrap enabled
  */
  if (plat->cfg.enable_mouse && plat->caps.supports_mouse) {
    (void)zr_posix_write_cstr(plat->stdout_fd, "\x1b[?1006l\x1b[?1003l\x1b[?1002l\x1b[?1000l");
  }
  if (plat->cfg.enable_bracketed_paste && plat->caps.supports_bracketed_paste) {
    (void)zr_posix_write_cstr(plat->stdout_fd, "\x1b[?2004l");
  }

  (void)zr_posix_write_cstr(plat->stdout_fd, "\x1b[?7h");
  (void)zr_posix_write_cstr(plat->stdout_fd, "\x1b[?25h");
  (void)zr_posix_write_cstr(plat->stdout_fd, "\x1b[?1049l");
}

/* Create POSIX platform handle with self-pipe wake and SIGWINCH handler. */
zr_result_t zr_plat_posix_create(plat_t** out_plat, const plat_config_t* cfg) {
  if (!out_plat || !cfg) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_plat = NULL;

  /*
    SIGWINCH wake is handled via a process-global signal handler, which needs a
    single global self-pipe write fd. Disallow multiple concurrent plat
    instances to avoid clobbering this global.
  */
  if (g_posix_wake_write_fd >= 0) {
    return ZR_ERR_PLATFORM;
  }

  plat_t* plat = (plat_t*)calloc(1u, sizeof(*plat));
  if (!plat) {
    return ZR_ERR_OOM;
  }

  zr_result_t r = ZR_ERR_PLATFORM;

  plat->cfg = *cfg;
  plat->stdin_fd = STDIN_FILENO;
  plat->stdout_fd = STDOUT_FILENO;
  plat->tty_fd_owned = -1;
  plat->wake_read_fd = -1;
  plat->wake_write_fd = -1;

  /*
    Some launchers (certain npm/WSL setups, IDE tasks, etc.) start Node with
    stdin/stdout not attached to the controlling terminal even though a TTY
    exists for interactive use. Raw mode requires a TTY for termios/ioctl.

    If either standard stream is not a TTY, fall back to /dev/tty so the
    engine can still render/interact with the controlling terminal.
  */
  if (isatty(plat->stdin_fd) == 0 || isatty(plat->stdout_fd) == 0) {
    int fd = open("/dev/tty", O_RDWR | O_NOCTTY);
    if (fd < 0) {
      r = ZR_ERR_PLATFORM;
      goto cleanup;
    }
    (void)zr_posix_set_fd_cloexec(fd);
    plat->tty_fd_owned = fd;
    plat->stdin_fd = fd;
    plat->stdout_fd = fd;
  }

  plat->caps.color_mode = cfg->requested_color_mode;
  plat->caps.supports_mouse = 1u;
  plat->caps.supports_bracketed_paste = 1u;
  /* Focus in/out bytes are not normalized by the core parser in v1. */
  plat->caps.supports_focus_events = 0u;
  plat->caps.supports_osc52 = 1u;
  plat->caps.supports_sync_update = zr_posix_detect_sync_update();
  plat->caps.supports_scroll_region = zr_posix_detect_scroll_region();
  plat->caps.supports_cursor_shape = zr_posix_term_is_dumb() ? 0u : 1u;
  plat->caps.supports_output_wait_writable = 1u;
  plat->caps._pad0[0] = 0u;
  plat->caps._pad0[1] = 0u;
  plat->caps._pad0[2] = 0u;
  plat->caps.sgr_attrs_supported = 0xFFFFFFFFu;

  r = zr_posix_make_self_pipe(&plat->wake_read_fd, &plat->wake_write_fd);
  if (r != ZR_OK) {
    goto cleanup;
  }

  g_posix_wake_write_fd = plat->wake_write_fd;

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = zr_posix_sigwinch_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGWINCH, &sa, &plat->sigwinch_prev) != 0) {
    r = ZR_ERR_PLATFORM;
    goto cleanup;
  }
  plat->sigwinch_installed = true;

  *out_plat = plat;
  return ZR_OK;

cleanup:
  if (g_posix_wake_write_fd == plat->wake_write_fd) {
    g_posix_wake_write_fd = -1;
  }

  if (plat->wake_read_fd >= 0) {
    (void)close(plat->wake_read_fd);
    plat->wake_read_fd = -1;
  }
  if (plat->wake_write_fd >= 0) {
    (void)close(plat->wake_write_fd);
    plat->wake_write_fd = -1;
  }

  if (plat->tty_fd_owned >= 0) {
    (void)close(plat->tty_fd_owned);
    plat->tty_fd_owned = -1;
  }

  free(plat);
  return r;
}

void plat_destroy(plat_t* plat) {
  if (!plat) {
    return;
  }

  (void)plat_leave_raw(plat);

  if (plat->sigwinch_installed) {
    /*
      With the singleton create guard, we should always own the global wake fd
      while SIGWINCH is installed.
    */
    ZR_ASSERT(g_posix_wake_write_fd == -1 || g_posix_wake_write_fd == plat->wake_write_fd);
    if (g_posix_wake_write_fd == plat->wake_write_fd) {
      g_posix_wake_write_fd = -1;
    }
    (void)sigaction(SIGWINCH, &plat->sigwinch_prev, NULL);
    plat->sigwinch_installed = false;
  }

  if (plat->wake_read_fd >= 0) {
    (void)close(plat->wake_read_fd);
    plat->wake_read_fd = -1;
  }
  if (plat->wake_write_fd >= 0) {
    (void)close(plat->wake_write_fd);
    plat->wake_write_fd = -1;
  }

  if (plat->tty_fd_owned >= 0) {
    (void)close(plat->tty_fd_owned);
    plat->tty_fd_owned = -1;
  }

  free(plat);
}

/* Enter raw terminal mode: disable echo/canonical, enable alt screen and mouse. */
zr_result_t plat_enter_raw(plat_t* plat) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (plat->raw_active) {
    return ZR_OK;
  }

  if (!plat->termios_valid) {
    if (tcgetattr(plat->stdin_fd, &plat->termios_saved) != 0) {
      return ZR_ERR_PLATFORM;
    }
    plat->termios_valid = true;
  }
  if (!plat->stdin_flags_valid) {
    int flags = fcntl(plat->stdin_fd, F_GETFL, 0);
    if (flags < 0) {
      return ZR_ERR_PLATFORM;
    }
    plat->stdin_flags_saved = flags;
    plat->stdin_flags_valid = true;
  }

  struct termios raw = plat->termios_saved;
  raw.c_iflag &= (tcflag_t) ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= (tcflag_t) ~(OPOST);
  raw.c_cflag |= (tcflag_t)(CS8);
  raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;

  if (tcsetattr(plat->stdin_fd, TCSANOW, &raw) != 0) {
    return ZR_ERR_PLATFORM;
  }

  if (zr_posix_set_fd_flag(plat->stdin_fd, O_NONBLOCK, true) != ZR_OK) {
    (void)tcsetattr(plat->stdin_fd, TCSANOW, &plat->termios_saved);
    return ZR_ERR_PLATFORM;
  }

  zr_posix_emit_enter_sequences(plat);
  plat->raw_active = true;
  return ZR_OK;
}

/* Leave raw mode: restore saved termios, leave alt screen, show cursor. Idempotent. */
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
    zr_posix_emit_leave_sequences(plat);
  }

  if (plat->termios_valid) {
    (void)tcsetattr(plat->stdin_fd, TCSANOW, &plat->termios_saved);
  }
  if (plat->stdin_flags_valid) {
    (void)fcntl(plat->stdin_fd, F_SETFL, plat->stdin_flags_saved);
  }

  plat->raw_active = false;
  return ZR_OK;
}

zr_result_t plat_get_size(plat_t* plat, plat_size_t* out_size) {
  if (!plat || !out_size) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  struct winsize ws;
  memset(&ws, 0, sizeof(ws));
  if (ioctl(plat->stdout_fd, TIOCGWINSZ, &ws) != 0) {
    out_size->cols = 0u;
    out_size->rows = 0u;
    return ZR_ERR_PLATFORM;
  }

  out_size->cols = (uint32_t)ws.ws_col;
  out_size->rows = (uint32_t)ws.ws_row;
  return ZR_OK;
}

zr_result_t plat_get_caps(plat_t* plat, plat_caps_t* out_caps) {
  if (!plat || !out_caps) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_caps = plat->caps;
  return ZR_OK;
}

/* Non-blocking read from stdin; returns bytes read, 0 if nothing available, or error. */
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

  for (;;) {
    ssize_t n = read(plat->stdin_fd, out_buf, (size_t)out_cap);
    if (n > 0) {
      if (n > (ssize_t)INT32_MAX) {
        return (int32_t)ZR_ERR_PLATFORM;
      }
      return (int32_t)n;
    }
    if (n == 0) {
      return 0;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;
    }
    return (int32_t)ZR_ERR_PLATFORM;
  }
}

zr_result_t plat_write_output(plat_t* plat, const uint8_t* bytes, int32_t len) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_posix_write_all(plat->stdout_fd, bytes, len);
}

/* Wait for input or wake signal; returns 1 if ready, 0 on timeout, or error code. */
int32_t plat_wait(plat_t* plat, int32_t timeout_ms) {
  if (!plat) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }
  if (timeout_ms < 0) {
    timeout_ms = -1;
  }

  uint64_t deadline_ms = 0ull;
  if (timeout_ms >= 0) {
    deadline_ms = plat_now_ms() + (uint64_t)timeout_ms;
  }

  struct pollfd fds[2];
  fds[0].fd = plat->stdin_fd;
  fds[0].events = POLLIN;
  fds[0].revents = 0;
  fds[1].fd = plat->wake_read_fd;
  fds[1].events = POLLIN;
  fds[1].revents = 0;

  for (;;) {
    if (g_posix_sigwinch_pending) {
      g_posix_sigwinch_pending = 0;
      zr_posix_drain_fd_best_effort(plat->wake_read_fd);
      return 1;
    }

    int poll_timeout = -1;
    if (timeout_ms >= 0) {
      uint64_t now_ms = plat_now_ms();
      uint64_t remaining = (now_ms >= deadline_ms) ? 0ull : (deadline_ms - now_ms);
      poll_timeout = remaining > (uint64_t)INT_MAX ? INT_MAX : (int)remaining;
    }

    fds[0].revents = 0;
    fds[1].revents = 0;
    int rc = poll(fds, 2u, poll_timeout);
    if (rc == 0) {
      return 0;
    }
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }
      return (int32_t)ZR_ERR_PLATFORM;
    }

    if ((fds[1].revents & POLLIN) != 0) {
      zr_posix_drain_fd_best_effort(plat->wake_read_fd);
      return 1;
    }
    if ((fds[0].revents & POLLIN) != 0) {
      return 1;
    }

    if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 ||
        (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      return (int32_t)ZR_ERR_PLATFORM;
    }
  }
}

/* Wake a blocked plat_wait call from another thread by writing to the self-pipe. */
zr_result_t plat_wake(plat_t* plat) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  const uint8_t b = 0u;
  for (;;) {
    ssize_t n = write(plat->wake_write_fd, &b, 1u);
    if (n == 1) {
      return ZR_OK;
    }
    if (n < 0 && errno == EINTR) {
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      /* Pipe already contains a wake byte; that's sufficient. */
      return ZR_OK;
    }
    return ZR_ERR_PLATFORM;
  }
}

uint64_t plat_now_ms(void) {
#if defined(CLOCK_MONOTONIC)
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0ull;
  }
  uint64_t ms = (uint64_t)ts.tv_sec * 1000ull;
  ms += (uint64_t)ts.tv_nsec / 1000000ull;
  return ms;
#else
  return 0ull;
#endif
}
