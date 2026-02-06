/*
  tests/integration/posix_caps_overrides.c — POSIX capability override environment contract.

  Why: Ensures non-standard terminals and CI harnesses can force capability
  bits deterministically via documented `ZIREAEL_CAP_*` environment overrides.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

  if (setenv("ZIREAEL_CAP_MOUSE", "0", 1) != 0 || setenv("ZIREAEL_CAP_BRACKETED_PASTE", "0", 1) != 0 ||
      setenv("ZIREAEL_CAP_OSC52", "0", 1) != 0 || setenv("ZIREAEL_CAP_SYNC_UPDATE", "1", 1) != 0 ||
      setenv("ZIREAEL_CAP_SCROLL_REGION", "0", 1) != 0 || setenv("ZIREAEL_CAP_CURSOR_SHAPE", "0", 1) != 0) {
    fprintf(stderr, "setenv() failed: errno=%d\n", errno);
    (void)close(master_fd);
    return 2;
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

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  r = plat_get_caps(plat, &caps);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_get_caps() failed: r=%d\n", (int)r);
    plat_destroy(plat);
    (void)close(master_fd);
    return 2;
  }

  if (caps.supports_mouse != 0u || caps.supports_bracketed_paste != 0u || caps.supports_osc52 != 0u ||
      caps.supports_sync_update != 1u || caps.supports_scroll_region != 0u || caps.supports_cursor_shape != 0u) {
    fprintf(stderr, "override mismatch: mouse=%u paste=%u osc52=%u sync=%u scroll=%u cursor=%u\n",
            (unsigned)caps.supports_mouse, (unsigned)caps.supports_bracketed_paste, (unsigned)caps.supports_osc52,
            (unsigned)caps.supports_sync_update, (unsigned)caps.supports_scroll_region,
            (unsigned)caps.supports_cursor_shape);
    plat_destroy(plat);
    (void)close(master_fd);
    return 2;
  }

  plat_destroy(plat);
  (void)unsetenv("ZIREAEL_CAP_MOUSE");
  (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
  (void)unsetenv("ZIREAEL_CAP_OSC52");
  (void)unsetenv("ZIREAEL_CAP_SYNC_UPDATE");
  (void)unsetenv("ZIREAEL_CAP_SCROLL_REGION");
  (void)unsetenv("ZIREAEL_CAP_CURSOR_SHAPE");
  (void)close(master_fd);
  return 0;
}
