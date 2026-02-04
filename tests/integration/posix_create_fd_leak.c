/*
  tests/integration/posix_create_fd_leak.c — POSIX backend create() failure cleanup.

  Why: When stdin/stdout are not TTYs but a controlling terminal exists, the POSIX
  backend falls back to opening `/dev/tty`. If subsequent initialization fails
  (e.g. self-pipe creation), the backend must close any owned fds before returning
  an error to avoid leaking resources across retries.
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

static int zr_child_main(int master_fd, int slave_fd) {
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

  int in_pipe[2] = {-1, -1};
  int out_pipe[2] = {-1, -1};
  if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
    return 2;
  }

  if (dup2(in_pipe[0], STDIN_FILENO) < 0) {
    return 2;
  }
  if (dup2(out_pipe[1], STDOUT_FILENO) < 0) {
    return 2;
  }

  (void)close(in_pipe[0]);
  (void)close(in_pipe[1]);
  (void)close(out_pipe[0]);
  (void)close(out_pipe[1]);

  /*
    Ensure we can still open one fd for `/dev/tty`, but not two fds for the
    backend self-pipe. With only fds {0,1,2} open and RLIMIT_NOFILE==4:
      - open(/dev/tty) succeeds (fd 3)
      - pipe() fails with EMFILE
  */
  zr_close_from(3);
  if (zr_set_nofile_limit(4) != 0) {
    return zr_test_skip("setrlimit(RLIMIT_NOFILE) failed");
  }

  plat_t* plat = NULL;
  plat_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.requested_color_mode = PLAT_COLOR_MODE_UNKNOWN;
  cfg.enable_mouse = 0u;
  cfg.enable_bracketed_paste = 0u;
  cfg.enable_focus_events = 0u;
  cfg.enable_osc52 = 0u;

  zr_result_t r = plat_create(&plat, &cfg);
  if (r == ZR_OK) {
    plat_destroy(plat);
    return 2;
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

int main(void) {
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
    return zr_child_main(master_fd, slave_fd);
  }

  /*
    Keep the PTY master open until the child exits so the slave remains valid as
    its controlling terminal during the test.
  */
  (void)close(slave_fd);

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    (void)close(master_fd);
    return 2;
  }
  (void)close(master_fd);
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 2;
}
