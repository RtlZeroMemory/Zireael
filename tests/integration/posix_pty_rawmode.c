/*
  tests/integration/posix_pty_rawmode.c — PTY raw-mode sequencing and SIGPIPE-safe writes.

  Why: Validates deterministic enter/leave VT sequence ordering, idempotent
  leave behavior, and broken-pipe write handling for the POSIX backend.
*/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "platform/zr_platform.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
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

/*
  Validate that plat_write_output() handles broken pipes deterministically.

  Why: With SIGPIPE at default disposition, writing to a reader-closed pipe
  would normally terminate the process. The platform backend must instead
  return ZR_ERR_PLATFORM and keep the process alive.
*/
typedef struct zr_broken_pipe_probe_t {
  int pipe_fds[2];
  int saved_stdout;
  bool restore_sigpipe;
  struct sigaction old_sigpipe;
} zr_broken_pipe_probe_t;

static void zr_restore_broken_pipe_probe(zr_broken_pipe_probe_t* probe) {
  if (!probe) {
    return;
  }
  if (probe->restore_sigpipe) {
    (void)sigaction(SIGPIPE, &probe->old_sigpipe, NULL);
    probe->restore_sigpipe = false;
  }
  if (probe->saved_stdout >= 0) {
    (void)dup2(probe->saved_stdout, STDOUT_FILENO);
    (void)close(probe->saved_stdout);
    probe->saved_stdout = -1;
  }
  if (probe->pipe_fds[0] >= 0) {
    (void)close(probe->pipe_fds[0]);
    probe->pipe_fds[0] = -1;
  }
  if (probe->pipe_fds[1] >= 0) {
    (void)close(probe->pipe_fds[1]);
    probe->pipe_fds[1] = -1;
  }
}

static int zr_begin_broken_pipe_probe(zr_broken_pipe_probe_t* probe) {
  if (!probe) {
    return -1;
  }
  probe->pipe_fds[0] = -1;
  probe->pipe_fds[1] = -1;
  probe->saved_stdout = -1;
  probe->restore_sigpipe = false;
  memset(&probe->old_sigpipe, 0, sizeof(probe->old_sigpipe));

  if (pipe(probe->pipe_fds) != 0) {
    goto fail;
  }
  if (close(probe->pipe_fds[0]) != 0) {
    goto fail;
  }
  probe->pipe_fds[0] = -1;

  probe->saved_stdout = dup(STDOUT_FILENO);
  if (probe->saved_stdout < 0) {
    goto fail;
  }
  if (dup2(probe->pipe_fds[1], STDOUT_FILENO) < 0) {
    goto fail;
  }
  (void)close(probe->pipe_fds[1]);
  probe->pipe_fds[1] = -1;

  struct sigaction sa_default;
  memset(&sa_default, 0, sizeof(sa_default));
  sa_default.sa_handler = SIG_DFL;
  sigemptyset(&sa_default.sa_mask);
  sa_default.sa_flags = 0;
  if (sigaction(SIGPIPE, &sa_default, &probe->old_sigpipe) != 0) {
    goto fail;
  }
  probe->restore_sigpipe = true;
  return 0;

fail:
  zr_restore_broken_pipe_probe(probe);
  return -1;
}

static int zr_expect_broken_pipe_platform_error(plat_t* plat) {
  if (!plat) {
    return -1;
  }

  zr_broken_pipe_probe_t probe;
  if (zr_begin_broken_pipe_probe(&probe) != 0) {
    return -1;
  }

  static const uint8_t payload[] = {0x41u};
  zr_result_t r = plat_write_output(plat, payload, (int32_t)sizeof(payload));
  if (r != ZR_ERR_PLATFORM) {
    fprintf(stderr, "plat_write_output() on broken pipe returned %d (expected %d)\n", (int)r, (int)ZR_ERR_PLATFORM);
    zr_restore_broken_pipe_probe(&probe);
    return -1;
  }

  zr_restore_broken_pipe_probe(&probe);
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
                                          "\x1b[r"
                                          "\x1b[0m"
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

  if (zr_expect_broken_pipe_platform_error(plat) != 0) {
    fprintf(stderr, "broken-pipe write regression check failed\n");
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
