/*
  tests/integration/posix_pty_wake.c — PTY-based wake behavior for plat_wait/plat_wake.

  Why: Ensures plat_wait is wakeable via the self-pipe wake mechanism from:
    - plat_wake() (other threads)
    - SIGWINCH handler (async-signal-safe wake path)
    - multiple concurrent POSIX platform instances
*/

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "platform/zr_platform.h"
#include "platform/posix/zr_plat_posix_test.h"

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
  plat_t* plat;
  int32_t timeout_ms;
  int32_t result;
} zr_wait_thread_args_t;

static volatile sig_atomic_t g_prev_sigwinch_count = 0;

static void zr_prev_sigwinch_handler(int signo) {
  (void)signo;
  g_prev_sigwinch_count++;
}

static void* zr_wait_thread(void* user) {
  zr_wait_thread_args_t* args = (zr_wait_thread_args_t*)user;
  args->result = plat_wait(args->plat, args->timeout_ms);
  return NULL;
}

static int zr_clear_ready_best_effort(plat_t* plat) {
  for (int i = 0; i < 16; i++) {
    int32_t w = plat_wait(plat, 0);
    if (w == 0) {
      return 0;
    }
    if (w < 0) {
      fprintf(stderr, "plat_wait(0) returned error while clearing: %d\n", (int)w);
      return -1;
    }
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

static int zr_expect_sigwinch_wake_preserved_on_forced_overflow(plat_t* plat) {
  if (!plat) {
    return -1;
  }
  if (zr_clear_ready_best_effort(plat) != 0) {
    return -1;
  }

  /*
    Seed the wake pipe with one byte, then force the SIGWINCH handler down the
    overflow-marker path. The next waits must observe: pipe wake, overflow wake,
    then timeout.
  */
  zr_result_t r = plat_wake(plat);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_wake() failed before overflow test: r=%d\n", (int)r);
    return -1;
  }

  const sig_atomic_t sig_count_before = g_prev_sigwinch_count;
  zr_posix_test_force_sigwinch_overflow(1u);
  (void)kill(getpid(), SIGWINCH);
  zr_posix_test_force_sigwinch_overflow(0u);

  if (g_prev_sigwinch_count != (sig_count_before + 1)) {
    fprintf(stderr, "SIGWINCH previous handler did not chain during overflow test (before=%d after=%d)\n",
            (int)sig_count_before, (int)g_prev_sigwinch_count);
    return -1;
  }

  int32_t w = plat_wait(plat, 0);
  if (w != 1) {
    fprintf(stderr, "expected wake-pipe readiness before overflow marker (result=%d)\n", (int)w);
    return -1;
  }

  w = plat_wait(plat, 0);
  if (w != 1) {
    fprintf(stderr, "lost SIGWINCH wake after wake-pipe drain (result=%d)\n", (int)w);
    return -1;
  }

  w = plat_wait(plat, 0);
  if (w != 0) {
    fprintf(stderr, "overflow wake marker was not single-shot (result=%d)\n", (int)w);
    return -1;
  }
  return 0;
}

int main(void) {
  int rc = 2;
  int master_fd = -1;
  int slave_fd = -1;
  plat_t* plat = NULL;
  plat_t* plat2 = NULL;
  bool sig_installed = false;
  struct sigaction saved_sigwinch;
  memset(&saved_sigwinch, 0, sizeof(saved_sigwinch));

  if (zr_make_pty_pair(&master_fd, &slave_fd) != 0) {
    return zr_test_skip("PTY APIs not available (posix_openpt/grantpt/unlockpt/ptsname/open)");
  }

  if (dup2(slave_fd, STDIN_FILENO) < 0 || dup2(slave_fd, STDOUT_FILENO) < 0) {
    fprintf(stderr, "dup2() failed: errno=%d\n", errno);
    goto cleanup;
  }
  if (slave_fd > STDOUT_FILENO) {
    (void)close(slave_fd);
    slave_fd = -1;
  }

  struct sigaction sa_prev;
  memset(&sa_prev, 0, sizeof(sa_prev));
  sa_prev.sa_handler = zr_prev_sigwinch_handler;
  sigemptyset(&sa_prev.sa_mask);
  sa_prev.sa_flags = 0;
  if (sigaction(SIGWINCH, &sa_prev, &saved_sigwinch) != 0) {
    fprintf(stderr, "sigaction(SIGWINCH install) failed: errno=%d\n", errno);
    goto cleanup;
  }
  sig_installed = true;

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
    goto cleanup;
  }

  /*
    Multi-instance support: second plat_create() should succeed and both
    instances must remain independently wakeable.
  */
  r = plat_create(&plat2, &cfg);
  if (r != ZR_OK || !plat2) {
    fprintf(stderr, "second plat_create() failed: r=%d plat2=%p\n", (int)r, (void*)plat2);
    goto cleanup;
  }

  if (zr_expect_wake_drains_pipe(plat) != 0) {
    fprintf(stderr, "wake pipe did not drain deterministically (plat1)\n");
    goto cleanup;
  }
  if (zr_expect_wake_drains_pipe(plat2) != 0) {
    fprintf(stderr, "wake pipe did not drain deterministically (plat2)\n");
    goto cleanup;
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
    goto cleanup;
  }

  zr_sleep_ms(50);
  r = plat_wake(plat);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_wake() failed: r=%d\n", (int)r);
    (void)pthread_join(th, NULL);
    goto cleanup;
  }

  (void)pthread_join(th, NULL);
  if (args.result != 1) {
    fprintf(stderr, "plat_wait() did not wake (result=%d)\n", (int)args.result);
    goto cleanup;
  }

  /* Signal wake: SIGWINCH must wake both platform instances. */
  zr_wait_thread_args_t args_a;
  zr_wait_thread_args_t args_b;
  memset(&args_a, 0, sizeof(args_a));
  memset(&args_b, 0, sizeof(args_b));
  args_a.plat = plat;
  args_a.timeout_ms = 5000;
  args_a.result = -999;
  args_b.plat = plat2;
  args_b.timeout_ms = 5000;
  args_b.result = -999;

  pthread_t th_a;
  pthread_t th_b;
  if (pthread_create(&th_a, NULL, zr_wait_thread, &args_a) != 0) {
    fprintf(stderr, "pthread_create() failed (signal test plat1)\n");
    goto cleanup;
  }
  if (pthread_create(&th_b, NULL, zr_wait_thread, &args_b) != 0) {
    fprintf(stderr, "pthread_create() failed (signal test plat2)\n");
    (void)pthread_join(th_a, NULL);
    goto cleanup;
  }

  const sig_atomic_t sig_count_before = g_prev_sigwinch_count;
  zr_sleep_ms(50);
  (void)kill(getpid(), SIGWINCH);
  (void)pthread_join(th_a, NULL);
  (void)pthread_join(th_b, NULL);
  if (args_a.result != 1 || args_b.result != 1) {
    fprintf(stderr, "plat_wait() did not wake on SIGWINCH (result1=%d result2=%d)\n", (int)args_a.result,
            (int)args_b.result);
    goto cleanup;
  }
  if (g_prev_sigwinch_count != (sig_count_before + 1)) {
    fprintf(stderr, "SIGWINCH previous handler did not chain (before=%d after=%d)\n", (int)sig_count_before,
            (int)g_prev_sigwinch_count);
    goto cleanup;
  }

  /*
    Destroy one instance: global SIGWINCH handler must remain active until the
    final instance is destroyed.
  */
  plat_destroy(plat);
  plat = NULL;

  memset(&args, 0, sizeof(args));
  args.plat = plat2;
  args.timeout_ms = 5000;
  args.result = -999;
  if (pthread_create(&th, NULL, zr_wait_thread, &args) != 0) {
    fprintf(stderr, "pthread_create() failed (remaining-instance signal test)\n");
    goto cleanup;
  }
  const sig_atomic_t sig_count_mid = g_prev_sigwinch_count;
  zr_sleep_ms(50);
  (void)kill(getpid(), SIGWINCH);
  (void)pthread_join(th, NULL);
  if (args.result != 1) {
    fprintf(stderr, "remaining instance did not wake on SIGWINCH (result=%d)\n", (int)args.result);
    goto cleanup;
  }
  if (g_prev_sigwinch_count != (sig_count_mid + 1)) {
    fprintf(stderr, "SIGWINCH previous handler did not chain after first destroy (before=%d after=%d)\n",
            (int)sig_count_mid, (int)g_prev_sigwinch_count);
    goto cleanup;
  }

  if (zr_expect_sigwinch_wake_preserved_on_forced_overflow(plat2) != 0) {
    fprintf(stderr, "SIGWINCH wake was not preserved across forced overflow path\n");
    goto cleanup;
  }

  plat_destroy(plat2);
  plat2 = NULL;

  /* Final destroy must restore the prior SIGWINCH handler we installed. */
  const sig_atomic_t restore_before = g_prev_sigwinch_count;
  (void)kill(getpid(), SIGWINCH);
  if (g_prev_sigwinch_count != (restore_before + 1)) {
    fprintf(stderr, "SIGWINCH handler was not restored on destroy (before=%d after=%d)\n", (int)restore_before,
            (int)g_prev_sigwinch_count);
    goto cleanup;
  }

  rc = 0;

cleanup:
  if (plat2) {
    plat_destroy(plat2);
    plat2 = NULL;
  }
  if (plat) {
    plat_destroy(plat);
    plat = NULL;
  }
  if (sig_installed) {
    (void)sigaction(SIGWINCH, &saved_sigwinch, NULL);
  }
  if (master_fd >= 0) {
    (void)close(master_fd);
  }
  if (slave_fd >= 0) {
    (void)close(slave_fd);
  }
  return rc;
}
