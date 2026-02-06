/*
  tests/integration/posix_pty_rawmode.c — PTY-based raw-mode enter/leave VT sequences.

  Why: Validates deterministic VT sequence ordering and idempotent leave behavior
  for the POSIX platform backend without requiring a real terminal emulator.
*/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "platform/zr_platform.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
  ZR_PTY_READ_TIMEOUT_MS = 5000,
};

static int zr_test_skip(const char* reason) {
  fprintf(stdout, "SKIP: %s\n", reason);
  return 77;
}

static int zr_poll_read_exact(int fd, uint8_t* out, size_t want, int timeout_ms) {
  size_t got = 0u;
  while (got < want) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int rc = poll(&pfd, 1u, timeout_ms);
    if (rc <= 0) {
      return -1;
    }
    if ((pfd.revents & POLLIN) == 0) {
      return -1;
    }

    ssize_t n = read(fd, out + got, want - got);
    if (n > 0) {
      got += (size_t)n;
      continue;
    }
    if (n < 0 && errno == EINTR) {
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      continue;
    }
    return -1;
  }
  return 0;
}

static int zr_poll_expect_no_more(int fd, int timeout_ms) {
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLIN;
  pfd.revents = 0;
  int rc = poll(&pfd, 1u, timeout_ms);
  if (rc == 0) {
    return 0;
  }
  return -1;
}

static int zr_make_pty_pair(int* out_master_fd, int* out_slave_fd) {
  if (!out_master_fd || !out_slave_fd) {
    return -1;
  }
  *out_master_fd = -1;
  *out_slave_fd = -1;

  int master_fd = posix_openpt(O_RDWR | O_NOCTTY);
  if (master_fd < 0) {
    return -1;
  }
  if (grantpt(master_fd) != 0) {
    (void)close(master_fd);
    return -1;
  }
  if (unlockpt(master_fd) != 0) {
    (void)close(master_fd);
    return -1;
  }

  const char* slave_name = ptsname(master_fd);
  if (!slave_name) {
    (void)close(master_fd);
    return -1;
  }

  int slave_fd = open(slave_name, O_RDWR | O_NOCTTY);
  if (slave_fd < 0) {
    (void)close(master_fd);
    return -1;
  }

  int master_flags = fcntl(master_fd, F_GETFL, 0);
  if (master_flags >= 0) {
    (void)fcntl(master_fd, F_SETFL, master_flags | O_NONBLOCK);
  }

  *out_master_fd = master_fd;
  *out_slave_fd = slave_fd;
  return 0;
}

int main(void) {
  int master_fd = -1;
  int slave_fd = -1;
  if (zr_make_pty_pair(&master_fd, &slave_fd) != 0) {
    return zr_test_skip("PTY APIs not available (posix_openpt/grantpt/unlockpt/ptsname/open)");
  }

  if (dup2(slave_fd, STDIN_FILENO) < 0 || dup2(slave_fd, STDOUT_FILENO) < 0) {
    fprintf(stderr, "dup2() failed: errno=%d\n", errno);
    (void)close(master_fd);
    (void)close(slave_fd);
    return 2;
  }
  if (slave_fd > STDOUT_FILENO) {
    (void)close(slave_fd);
    slave_fd = -1;
  }

  if (setenv("ZIREAEL_CAP_MOUSE", "1", 1) != 0 || setenv("ZIREAEL_CAP_BRACKETED_PASTE", "1", 1) != 0) {
    fprintf(stderr, "setenv() failed: errno=%d\n", errno);
    (void)close(master_fd);
    return 2;
  }

  plat_t* plat = NULL;
  plat_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.requested_color_mode = PLAT_COLOR_MODE_UNKNOWN;
  cfg.enable_mouse = 1u;
  cfg.enable_bracketed_paste = 1u;
  cfg.enable_focus_events = 0u;
  cfg.enable_osc52 = 0u;

  zr_result_t r = plat_create(&plat, &cfg);
  if (r != ZR_OK || !plat) {
    fprintf(stderr, "plat_create() failed: r=%d\n", (int)r);
    (void)unsetenv("ZIREAEL_CAP_MOUSE");
    (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
    (void)close(master_fd);
    return 2;
  }

  static const uint8_t expected_enter[] = "\x1b[?1049h"
                                          "\x1b[?25l"
                                          "\x1b[?7h"
                                          "\x1b[?2004h"
                                          "\x1b[?1000h\x1b[?1002h\x1b[?1003h\x1b[?1006h";

  static const uint8_t expected_leave[] = "\x1b[?1006l\x1b[?1003l\x1b[?1002l\x1b[?1000l"
                                          "\x1b[?2004l"
                                          "\x1b[?7h"
                                          "\x1b[?25h"
                                          "\x1b[?1049l";

  r = plat_enter_raw(plat);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_enter_raw() failed: r=%d\n", (int)r);
    plat_destroy(plat);
    (void)unsetenv("ZIREAEL_CAP_MOUSE");
    (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
    (void)close(master_fd);
    return 2;
  }

  uint8_t got_enter[sizeof(expected_enter) - 1u];
  memset(got_enter, 0, sizeof(got_enter));
  if (zr_poll_read_exact(master_fd, got_enter, sizeof(got_enter), ZR_PTY_READ_TIMEOUT_MS) != 0) {
    fprintf(stderr, "failed to read enter sequence from PTY\n");
    plat_destroy(plat);
    (void)unsetenv("ZIREAEL_CAP_MOUSE");
    (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
    (void)close(master_fd);
    return 2;
  }
  if (memcmp(got_enter, expected_enter, sizeof(got_enter)) != 0) {
    fprintf(stderr, "enter sequence mismatch\n");
    plat_destroy(plat);
    (void)unsetenv("ZIREAEL_CAP_MOUSE");
    (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
    (void)close(master_fd);
    return 2;
  }
  if (zr_poll_expect_no_more(master_fd, 50) != 0) {
    fprintf(stderr, "unexpected extra output after enter sequence\n");
    plat_destroy(plat);
    (void)unsetenv("ZIREAEL_CAP_MOUSE");
    (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
    (void)close(master_fd);
    return 2;
  }

  r = plat_leave_raw(plat);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_leave_raw() failed: r=%d\n", (int)r);
    plat_destroy(plat);
    (void)unsetenv("ZIREAEL_CAP_MOUSE");
    (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
    (void)close(master_fd);
    return 2;
  }

  uint8_t got_leave[sizeof(expected_leave) - 1u];
  memset(got_leave, 0, sizeof(got_leave));
  if (zr_poll_read_exact(master_fd, got_leave, sizeof(got_leave), ZR_PTY_READ_TIMEOUT_MS) != 0) {
    fprintf(stderr, "failed to read leave sequence from PTY\n");
    plat_destroy(plat);
    (void)unsetenv("ZIREAEL_CAP_MOUSE");
    (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
    (void)close(master_fd);
    return 2;
  }
  if (memcmp(got_leave, expected_leave, sizeof(got_leave)) != 0) {
    fprintf(stderr, "leave sequence mismatch\n");
    plat_destroy(plat);
    (void)unsetenv("ZIREAEL_CAP_MOUSE");
    (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
    (void)close(master_fd);
    return 2;
  }

  /* Idempotence: second leave must be safe and return OK. */
  r = plat_leave_raw(plat);
  if (r != ZR_OK) {
    fprintf(stderr, "second plat_leave_raw() failed: r=%d\n", (int)r);
    plat_destroy(plat);
    (void)unsetenv("ZIREAEL_CAP_MOUSE");
    (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
    (void)close(master_fd);
    return 2;
  }

  plat_destroy(plat);
  (void)unsetenv("ZIREAEL_CAP_MOUSE");
  (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
  (void)close(master_fd);
  return 0;
}
