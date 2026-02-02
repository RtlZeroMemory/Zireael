/*
  tests/integration/posix_pty_wake.c — PTY-based wake behavior for plat_wait/plat_wake.

  Why: Ensures plat_wait is wakeable via the self-pipe wake mechanism from:
    - plat_wake() (other threads)
    - SIGWINCH handler (async-signal-safe wake path)
*/

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "platform/zr_platform.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int zr_test_skip(const char* reason) {
  fprintf(stdout, "SKIP: %s\n", reason);
  return 77;
}

static void zr_sleep_ms(int32_t ms) {
  if (ms <= 0) {
    return;
  }
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000);
  ts.tv_nsec = (long)(ms % 1000) * 1000000l;
  while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
  }
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

typedef struct zr_wait_thread_args_t {
  plat_t*  plat;
  int32_t  timeout_ms;
  int32_t  result;
} zr_wait_thread_args_t;

static void* zr_wait_thread(void* user) {
  zr_wait_thread_args_t* args = (zr_wait_thread_args_t*)user;
  args->result = plat_wait(args->plat, args->timeout_ms);
  return NULL;
}

static int zr_clear_ready_best_effort(plat_t* plat) {
  uint8_t tmp[256];

  for (int i = 0; i < 16; i++) {
    int32_t w = plat_wait(plat, 0);
    if (w == 0) {
      return 0;
    }
    if (w < 0) {
      fprintf(stderr, "plat_wait(0) returned error while clearing: %d\n", (int)w);
      return -1;
    }
    (void)plat_read_input(plat, tmp, (int32_t)sizeof(tmp));
  }
  fprintf(stderr, "plat_wait(0) never settled to timeout while clearing\n");
  return -1;
}

static int zr_expect_wake_drains_pipe(plat_t* plat) {
  if (zr_clear_ready_best_effort(plat) != 0) {
    return -1;
  }

  zr_result_t r = plat_wake(plat);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_wake() failed: r=%d\n", (int)r);
    return -1;
  }
  r = plat_wake(plat);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_wake() failed (second): r=%d\n", (int)r);
    return -1;
  }

  int32_t w = plat_wait(plat, 0);
  if (w != 1) {
    fprintf(stderr, "plat_wait(0) after wake returned %d\n", (int)w);
    return -1;
  }

  /*
    After consuming one "woke" readiness, the self-pipe must not cause
    indefinite ready status.
  */
  if (zr_clear_ready_best_effort(plat) != 0) {
    return -1;
  }
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

  plat_t* plat = NULL;
  plat_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.requested_color_mode = PLAT_COLOR_MODE_UNKNOWN;
  cfg.enable_mouse = 0u;
  cfg.enable_bracketed_paste = 0u;
  cfg.enable_focus_events = 0u;
  cfg.enable_osc52 = 0u;

  zr_result_t r = plat_create(&plat, &cfg);
  if (r != ZR_OK || !plat) {
    fprintf(stderr, "plat_create() failed: r=%d\n", (int)r);
    (void)close(master_fd);
    return 2;
  }

  /* Singleton guard: POSIX backend is currently process-singleton (SIGWINCH + global wake fd). */
  plat_t* plat2 = NULL;
  r = plat_create(&plat2, &cfg);
  if (r != ZR_ERR_PLATFORM || plat2 != NULL) {
    fprintf(stderr, "expected second plat_create() to fail with ZR_ERR_PLATFORM (r=%d plat2=%p)\n", (int)r, (void*)plat2);
    if (plat2) {
      plat_destroy(plat2);
    }
    plat_destroy(plat);
    (void)close(master_fd);
    return 2;
  }

  if (zr_expect_wake_drains_pipe(plat) != 0) {
    fprintf(stderr, "wake pipe did not drain deterministically\n");
    plat_destroy(plat);
    (void)close(master_fd);
    return 2;
  }

  /* Thread wake: plat_wait must return promptly after plat_wake from another thread. */
  zr_wait_thread_args_t args;
  memset(&args, 0, sizeof(args));
  args.plat = plat;
  args.timeout_ms = 5000;
  args.result = -999;

  pthread_t th;
  if (pthread_create(&th, NULL, zr_wait_thread, &args) != 0) {
    fprintf(stderr, "pthread_create() failed\n");
    plat_destroy(plat);
    (void)close(master_fd);
    return 2;
  }

  zr_sleep_ms(50);
  r = plat_wake(plat);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_wake() failed: r=%d\n", (int)r);
    (void)pthread_join(th, NULL);
    plat_destroy(plat);
    (void)close(master_fd);
    return 2;
  }

  (void)pthread_join(th, NULL);
  if (args.result != 1) {
    fprintf(stderr, "plat_wait() did not wake (result=%d)\n", (int)args.result);
    plat_destroy(plat);
    (void)close(master_fd);
    return 2;
  }

  /* Signal wake: SIGWINCH handler must wake plat_wait through the self-pipe. */
  memset(&args, 0, sizeof(args));
  args.plat = plat;
  args.timeout_ms = 5000;
  args.result = -999;

  if (pthread_create(&th, NULL, zr_wait_thread, &args) != 0) {
    fprintf(stderr, "pthread_create() failed (signal test)\n");
    plat_destroy(plat);
    (void)close(master_fd);
    return 2;
  }

  zr_sleep_ms(50);
  (void)kill(getpid(), SIGWINCH);
  (void)pthread_join(th, NULL);
  if (args.result != 1) {
    fprintf(stderr, "plat_wait() did not wake on SIGWINCH (result=%d)\n", (int)args.result);
    plat_destroy(plat);
    (void)close(master_fd);
    return 2;
  }

  plat_destroy(plat);
  (void)close(master_fd);
  return 0;
}
