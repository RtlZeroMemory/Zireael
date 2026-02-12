/*
  tests/integration/posix_create_fd_leak.c — POSIX create() fallback cleanup + explicit pipe mode.

  Why: Covers two deterministic regressions in non-TTY launch paths:
    - `/dev/tty` fallback failure must not leak owned fds
    - explicit non-TTY pipe mode must allow create/raw/size without termios/ioctl-on-pipe failures
*/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "platform/zr_platform.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum {
  ZR_NOFILE_LIMIT_FORCE_TTY_FALLBACK_FAIL = 4u,
  ZR_NOFILE_LIMIT_PIPE_MODE_BYPASS = 5u,
  ZR_PIPE_MODE_EXPECTED_COLS = 80u,
  ZR_PIPE_MODE_EXPECTED_ROWS = 24u,
};

static int zr_test_skip(const char* reason) {
  fprintf(stdout, "SKIP: %s\n", reason);
  return 77;
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

  *out_master_fd = master_fd;
  *out_slave_fd = slave_fd;
  return 0;
}

static int zr_set_nofile_limit(rlim_t limit) {
  struct rlimit rl;
  memset(&rl, 0, sizeof(rl));
  rl.rlim_cur = limit;
  rl.rlim_max = limit;
  return setrlimit(RLIMIT_NOFILE, &rl);
}

static void zr_close_from(int first_fd) {
  long max_fd = sysconf(_SC_OPEN_MAX);
  if (max_fd < 0) {
    max_fd = 256;
  }
  for (int fd = first_fd; fd < max_fd; fd++) {
    (void)close(fd);
  }
}

static void zr_init_default_cfg(plat_config_t* cfg) {
  if (!cfg) {
    return;
  }
  memset(cfg, 0, sizeof(*cfg));
  cfg->requested_color_mode = PLAT_COLOR_MODE_UNKNOWN;
  cfg->enable_mouse = 0u;
  cfg->enable_bracketed_paste = 0u;
  cfg->enable_focus_events = 0u;
  cfg->enable_osc52 = 0u;
}

static int zr_redirect_stdio_to_pipes(void) {
  int in_pipe[2] = {-1, -1};
  int out_pipe[2] = {-1, -1};
  if (pipe(in_pipe) != 0) {
    return -1;
  }
  if (pipe(out_pipe) != 0) {
    (void)close(in_pipe[0]);
    (void)close(in_pipe[1]);
    return -1;
  }

  if (dup2(in_pipe[0], STDIN_FILENO) < 0 || dup2(out_pipe[1], STDOUT_FILENO) < 0) {
    (void)close(in_pipe[0]);
    (void)close(in_pipe[1]);
    (void)close(out_pipe[0]);
    (void)close(out_pipe[1]);
    return -1;
  }

  (void)close(in_pipe[0]);
  (void)close(in_pipe[1]);
  (void)close(out_pipe[0]);
  (void)close(out_pipe[1]);
  return 0;
}

static int zr_expect_pipe_mode_contract(plat_t* plat) {
  if (!plat) {
    return -1;
  }

  zr_result_t r = plat_enter_raw(plat);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_enter_raw() in pipe mode returned %d\n", (int)r);
    return -1;
  }

  plat_size_t size;
  memset(&size, 0, sizeof(size));
  r = plat_get_size(plat, &size);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_get_size() in pipe mode returned %d\n", (int)r);
    return -1;
  }
  if (size.cols != ZR_PIPE_MODE_EXPECTED_COLS || size.rows != ZR_PIPE_MODE_EXPECTED_ROWS) {
    fprintf(stderr, "pipe-mode size mismatch: got=%ux%u expected=%ux%u\n", (unsigned)size.cols, (unsigned)size.rows,
            (unsigned)ZR_PIPE_MODE_EXPECTED_COLS, (unsigned)ZR_PIPE_MODE_EXPECTED_ROWS);
    return -1;
  }

  r = plat_leave_raw(plat);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_leave_raw() in pipe mode returned %d\n", (int)r);
    return -1;
  }
  return 0;
}

static int zr_child_fd_leak_regression(int master_fd, int slave_fd) {
  if (setsid() < 0) {
    return zr_test_skip("setsid() failed; cannot acquire a controlling terminal");
  }

  /*
    Acquire a controlling terminal for `/dev/tty`, then redirect stdio away from
    the PTY so plat_create() takes the `/dev/tty` fallback path.
  */
  if (master_fd >= 0) {
    (void)close(master_fd);
    master_fd = -1;
  }
  if (slave_fd < 0) {
    return zr_test_skip("PTY slave fd unavailable");
  }
  if (ioctl(slave_fd, TIOCSCTTY, 0) != 0) {
    (void)close(slave_fd);
    return zr_test_skip("ioctl(TIOCSCTTY) failed; cannot set controlling terminal");
  }
  (void)close(slave_fd);

  int tty_probe = open("/dev/tty", O_RDWR | O_NOCTTY);
  if (tty_probe < 0) {
    return zr_test_skip("open(/dev/tty) failed; no controlling terminal available");
  }
  (void)close(tty_probe);

  if (zr_redirect_stdio_to_pipes() != 0) {
    return 2;
  }

  /*
    Ensure we can still open one fd for `/dev/tty`, but not two fds for the
    backend self-pipe. With only fds {0,1,2} open and RLIMIT_NOFILE==4:
      - open(/dev/tty) succeeds (fd 3)
      - pipe() fails with EMFILE
  */
  zr_close_from(3);
  if (zr_set_nofile_limit(ZR_NOFILE_LIMIT_FORCE_TTY_FALLBACK_FAIL) != 0) {
    return zr_test_skip("setrlimit(RLIMIT_NOFILE) failed");
  }

  (void)unsetenv("ZIREAEL_POSIX_PIPE_MODE");

  plat_t* plat = NULL;
  plat_config_t cfg;
  zr_init_default_cfg(&cfg);

  zr_result_t r = plat_create(&plat, &cfg);
  if (r != ZR_ERR_PLATFORM) {
    fprintf(stderr, "plat_create() fallback-failure path returned %d (expected %d)\n", (int)r, (int)ZR_ERR_PLATFORM);
    if (r == ZR_OK && plat) {
      plat_destroy(plat);
    }
    return 2;
  }
  if (plat) {
    plat_destroy(plat);
  }

  /*
    Regression check: on this forced failure path, the backend must close the
    owned `/dev/tty` fd. If it leaks, opening any new fd under the 4-fd limit
    will fail with EMFILE.
  */
  int fd = open("/dev/null", O_RDONLY);
  if (fd < 0) {
    return 2;
  }
  (void)close(fd);
  return 0;
}

static int zr_child_pipe_mode_without_controlling_tty(void) {
  if (setsid() < 0) {
    return zr_test_skip("setsid() failed; cannot detach from controlling terminal");
  }
  if (zr_redirect_stdio_to_pipes() != 0) {
    return 2;
  }

  zr_close_from(3);
  (void)unsetenv("ZIREAEL_POSIX_PIPE_MODE");

  plat_t* plat = NULL;
  plat_config_t cfg;
  zr_init_default_cfg(&cfg);

  zr_result_t r = plat_create(&plat, &cfg);
  if (r != ZR_ERR_PLATFORM) {
    fprintf(stderr, "plat_create() without pipe mode returned %d (expected %d)\n", (int)r, (int)ZR_ERR_PLATFORM);
    if (r == ZR_OK && plat) {
      plat_destroy(plat);
    }
    return 2;
  }
  if (plat) {
    plat_destroy(plat);
    plat = NULL;
  }

  if (setenv("ZIREAEL_POSIX_PIPE_MODE", "1", 1) != 0) {
    fprintf(stderr, "setenv(ZIREAEL_POSIX_PIPE_MODE=1) failed: errno=%d\n", errno);
    return 2;
  }

  r = plat_create(&plat, &cfg);
  if (r != ZR_OK || !plat) {
    fprintf(stderr, "plat_create() with explicit pipe mode failed: r=%d\n", (int)r);
    return 2;
  }
  if (zr_expect_pipe_mode_contract(plat) != 0) {
    plat_destroy(plat);
    return 2;
  }

  plat_destroy(plat);
  (void)unsetenv("ZIREAEL_POSIX_PIPE_MODE");
  return 0;
}

static int zr_child_pipe_mode_skips_dev_tty(int master_fd, int slave_fd) {
  if (setsid() < 0) {
    return zr_test_skip("setsid() failed; cannot acquire a controlling terminal");
  }

  if (master_fd >= 0) {
    (void)close(master_fd);
    master_fd = -1;
  }
  if (slave_fd < 0) {
    return zr_test_skip("PTY slave fd unavailable");
  }
  if (ioctl(slave_fd, TIOCSCTTY, 0) != 0) {
    (void)close(slave_fd);
    return zr_test_skip("ioctl(TIOCSCTTY) failed; cannot set controlling terminal");
  }
  (void)close(slave_fd);

  int tty_probe = open("/dev/tty", O_RDWR | O_NOCTTY);
  if (tty_probe < 0) {
    return zr_test_skip("open(/dev/tty) failed; no controlling terminal available");
  }
  (void)close(tty_probe);

  if (zr_redirect_stdio_to_pipes() != 0) {
    return 2;
  }

  /*
    With RLIMIT_NOFILE==5 and only {0,1,2} open:
      - explicit pipe mode (no /dev/tty open) leaves room for self-pipe {3,4}
      - any /dev/tty fallback attempt consumes fd 3 and makes pipe() fail
  */
  zr_close_from(3);
  if (zr_set_nofile_limit(ZR_NOFILE_LIMIT_PIPE_MODE_BYPASS) != 0) {
    return zr_test_skip("setrlimit(RLIMIT_NOFILE) failed");
  }
  if (setenv("ZIREAEL_POSIX_PIPE_MODE", "1", 1) != 0) {
    fprintf(stderr, "setenv(ZIREAEL_POSIX_PIPE_MODE=1) failed: errno=%d\n", errno);
    return 2;
  }

  plat_t* plat = NULL;
  plat_config_t cfg;
  zr_init_default_cfg(&cfg);

  zr_result_t r = plat_create(&plat, &cfg);
  if (r != ZR_OK || !plat) {
    fprintf(stderr, "plat_create() in pipe mode + /dev/tty-available case failed: r=%d\n", (int)r);
    return 2;
  }
  if (zr_expect_pipe_mode_contract(plat) != 0) {
    plat_destroy(plat);
    return 2;
  }

  plat_destroy(plat);
  (void)unsetenv("ZIREAEL_POSIX_PIPE_MODE");
  return 0;
}

typedef int (*zr_child_plain_fn_t)(void);
typedef int (*zr_child_pty_fn_t)(int, int);

static int zr_wait_child_exit_status(pid_t pid) {
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    return 2;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 2;
}

static int zr_run_child_plain(zr_child_plain_fn_t fn) {
  if (!fn) {
    return 2;
  }
  pid_t pid = fork();
  if (pid < 0) {
    return 2;
  }
  if (pid == 0) {
    const int child_rc = fn();
    _exit((child_rc >= 0 && child_rc <= 255) ? child_rc : 2);
  }
  return zr_wait_child_exit_status(pid);
}

static int zr_run_child_with_pty(zr_child_pty_fn_t fn) {
  if (!fn) {
    return 2;
  }

  int master_fd = -1;
  int slave_fd = -1;
  if (zr_make_pty_pair(&master_fd, &slave_fd) != 0) {
    return zr_test_skip("PTY APIs not available (posix_openpt/grantpt/unlockpt/ptsname/open)");
  }

  pid_t pid = fork();
  if (pid < 0) {
    (void)close(master_fd);
    (void)close(slave_fd);
    return 2;
  }
  if (pid == 0) {
    const int child_rc = fn(master_fd, slave_fd);
    _exit((child_rc >= 0 && child_rc <= 255) ? child_rc : 2);
  }

  /*
    Keep the PTY master open until the child exits so the slave remains valid as
    its controlling terminal during the test.
  */
  (void)close(slave_fd);

  int rc = zr_wait_child_exit_status(pid);
  (void)close(master_fd);
  return rc;
}

int main(void) {
  int rc = zr_run_child_with_pty(zr_child_fd_leak_regression);
  if (rc != 0) {
    fprintf(stderr, "zr_child_fd_leak_regression failed: rc=%d\n", rc);
    return rc;
  }

  rc = zr_run_child_plain(zr_child_pipe_mode_without_controlling_tty);
  if (rc != 0) {
    fprintf(stderr, "zr_child_pipe_mode_without_controlling_tty failed: rc=%d\n", rc);
    return rc;
  }

  rc = zr_run_child_with_pty(zr_child_pipe_mode_skips_dev_tty);
  if (rc != 0) {
    fprintf(stderr, "zr_child_pipe_mode_skips_dev_tty failed: rc=%d\n", rc);
  }
  return rc;
}
