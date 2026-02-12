/*
  tests/integration/posix_caps_overrides.c — POSIX capability and color-mode env contract.

  Why: Ensures capability overrides and color-mode heuristics remain deterministic
  across TERM/COLORTERM/terminal-marker environment combinations, including
  requested-color clamping.
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

enum {
  ZR_STYLE_ATTR_BOLD = 1u << 0u,
  ZR_STYLE_ATTR_ITALIC = 1u << 1u,
  ZR_STYLE_ATTR_UNDERLINE = 1u << 2u,
  ZR_STYLE_ATTR_REVERSE = 1u << 3u,
  ZR_STYLE_ATTR_STRIKE = 1u << 4u,
  ZR_STYLE_ATTR_BASIC_MASK = ZR_STYLE_ATTR_BOLD | ZR_STYLE_ATTR_UNDERLINE | ZR_STYLE_ATTR_REVERSE,
  ZR_COLOR_REQUEST_COUNT = 4u,
};

typedef struct zr_color_env_case_t {
  const char* name;
  const char* term;
  const char* colorterm;
  const char* term_program;
  const char* kitty_window_id;
  const char* wezterm_pane;
  const char* wezterm_executable;
  const char* ghostty_resources_dir;
  const char* vte_version;
  const char* konsole_version;
  const char* wt_session;
} zr_color_env_case_t;

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

static int zr_env_set_optional(const char* key, const char* value) {
  if (!key) {
    return -1;
  }
  if (!value) {
    return unsetenv(key);
  }
  return setenv(key, value, 1);
}

static int zr_clear_color_detection_env(void) {
  static const char* kKeys[] = {"TERM",
                                "COLORTERM",
                                "TERM_PROGRAM",
                                "KITTY_WINDOW_ID",
                                "WEZTERM_PANE",
                                "WEZTERM_EXECUTABLE",
                                "GHOSTTY_RESOURCES_DIR",
                                "VTE_VERSION",
                                "KONSOLE_VERSION",
                                "WT_SESSION"};
  for (size_t i = 0u; i < sizeof(kKeys) / sizeof(kKeys[0]); i++) {
    if (zr_env_set_optional(kKeys[i], NULL) != 0) {
      fprintf(stderr, "unsetenv(%s) failed: errno=%d\n", kKeys[i], errno);
      return -1;
    }
  }
  return 0;
}

static int zr_apply_color_env_case(const zr_color_env_case_t* env_case) {
  if (!env_case) {
    return -1;
  }

  if (zr_clear_color_detection_env() != 0) {
    return -1;
  }
  if (zr_env_set_optional("TERM", env_case->term) != 0 || zr_env_set_optional("COLORTERM", env_case->colorterm) != 0 ||
      zr_env_set_optional("TERM_PROGRAM", env_case->term_program) != 0 ||
      zr_env_set_optional("KITTY_WINDOW_ID", env_case->kitty_window_id) != 0 ||
      zr_env_set_optional("WEZTERM_PANE", env_case->wezterm_pane) != 0 ||
      zr_env_set_optional("WEZTERM_EXECUTABLE", env_case->wezterm_executable) != 0 ||
      zr_env_set_optional("GHOSTTY_RESOURCES_DIR", env_case->ghostty_resources_dir) != 0 ||
      zr_env_set_optional("VTE_VERSION", env_case->vte_version) != 0 ||
      zr_env_set_optional("KONSOLE_VERSION", env_case->konsole_version) != 0 ||
      zr_env_set_optional("WT_SESSION", env_case->wt_session) != 0) {
    fprintf(stderr, "failed to apply color env case '%s': errno=%d\n", env_case->name ? env_case->name : "unnamed",
            errno);
    return -1;
  }
  return 0;
}

static void zr_clear_cap_override_env(void) {
  (void)unsetenv("ZIREAEL_CAP_MOUSE");
  (void)unsetenv("ZIREAEL_CAP_BRACKETED_PASTE");
  (void)unsetenv("ZIREAEL_CAP_OSC52");
  (void)unsetenv("ZIREAEL_CAP_SYNC_UPDATE");
  (void)unsetenv("ZIREAEL_CAP_SCROLL_REGION");
  (void)unsetenv("ZIREAEL_CAP_CURSOR_SHAPE");
  (void)unsetenv("ZIREAEL_CAP_FOCUS_EVENTS");
  (void)unsetenv("ZIREAEL_CAP_SGR_ATTRS");
  (void)unsetenv("ZIREAEL_CAP_SGR_ATTRS_MASK");
}

static int zr_set_cap_override_defaults(void) {
  if (setenv("ZIREAEL_CAP_MOUSE", "0", 1) != 0 || setenv("ZIREAEL_CAP_BRACKETED_PASTE", "0", 1) != 0 ||
      setenv("ZIREAEL_CAP_OSC52", "0", 1) != 0 || setenv("ZIREAEL_CAP_SYNC_UPDATE", "1", 1) != 0 ||
      setenv("ZIREAEL_CAP_SCROLL_REGION", "0", 1) != 0 || setenv("ZIREAEL_CAP_CURSOR_SHAPE", "0", 1) != 0 ||
      setenv("ZIREAEL_CAP_FOCUS_EVENTS", "0", 1) != 0) {
    fprintf(stderr, "setenv() capability override setup failed: errno=%d\n", errno);
    return -1;
  }
  return 0;
}

static int zr_read_caps_for_cfg(const plat_config_t* cfg, const char* context, plat_caps_t* out_caps) {
  if (!cfg || !out_caps) {
    return -1;
  }

  plat_t* plat = NULL;
  memset(out_caps, 0, sizeof(*out_caps));

  zr_result_t r = plat_create(&plat, cfg);
  if (r != ZR_OK || !plat) {
    fprintf(stderr, "plat_create() failed (%s): r=%d\n", context ? context : "n/a", (int)r);
    return -1;
  }

  r = plat_get_caps(plat, out_caps);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_get_caps() failed (%s): r=%d\n", context ? context : "n/a", (int)r);
    plat_destroy(plat);
    return -1;
  }

  plat_destroy(plat);
  return 0;
}

static int zr_expect_color_mode(const char* label, const zr_color_env_case_t* env_case, const plat_config_t* cfg,
                                plat_color_mode_t expected_color_mode) {
  plat_caps_t caps;
  if (zr_apply_color_env_case(env_case) != 0) {
    return -1;
  }
  if (zr_read_caps_for_cfg(cfg, label, &caps) != 0) {
    return -1;
  }

  if (caps.color_mode != expected_color_mode) {
    fprintf(stderr, "color_mode mismatch (%s): got=%u want=%u\n", label ? label : "n/a", (unsigned)caps.color_mode,
            (unsigned)expected_color_mode);
    return -1;
  }
  return 0;
}

static int zr_run_color_detection_matrix(const plat_config_t* base_cfg) {
  static const struct {
    zr_color_env_case_t env_case;
    plat_color_mode_t expected_mode;
  } kCases[] = {
      {.env_case = {.name = "term-unset-defaults-16"}, .expected_mode = PLAT_COLOR_MODE_16},
      {.env_case = {.name = "term-empty-defaults-16", .term = ""}, .expected_mode = PLAT_COLOR_MODE_16},
      {.env_case = {.name = "term-dumb-wins-over-truecolor", .term = "dumb", .colorterm = "truecolor"},
       .expected_mode = PLAT_COLOR_MODE_16},
      {.env_case = {.name = "term-256color-detects-256", .term = "xterm-256color"},
       .expected_mode = PLAT_COLOR_MODE_256},
      {.env_case = {.name = "term-256color-case-insensitive", .term = "XTERM-256COLOR"},
       .expected_mode = PLAT_COLOR_MODE_256},
      {.env_case = {.name = "colorterm-24-bit-promotes-rgb", .term = "xterm-256color", .colorterm = "24-bit"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "colorterm-rgb-promotes-rgb", .term = "linux", .colorterm = "RGB"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "term-direct-detects-rgb", .term = "xterm-direct"}, .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "term-24bit-token-detects-rgb", .term = "ansi-24bit"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "term-program-vscode-detects-rgb", .term = "vt100", .term_program = "VSCODE"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "term-program-wezterm-detects-rgb", .term = "vt100", .term_program = "WezTerm"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "kitty-env-detects-rgb", .term = "vt100", .kitty_window_id = "1"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "wezterm-pane-env-detects-rgb", .term = "vt100", .wezterm_pane = "42"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "wezterm-executable-env-detects-rgb",
                    .term = "vt100",
                    .wezterm_executable = "/usr/bin/wezterm"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "ghostty-env-detects-rgb", .term = "vt100", .ghostty_resources_dir = "/tmp/ghostty"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "vte-version-env-detects-rgb", .term = "vt100", .vte_version = "7600"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "konsole-version-env-detects-rgb", .term = "vt100", .konsole_version = "230800"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
      {.env_case = {.name = "wt-session-env-detects-rgb", .term = "vt100", .wt_session = "abc123"},
       .expected_mode = PLAT_COLOR_MODE_RGB},
  };

  for (size_t i = 0u; i < sizeof(kCases) / sizeof(kCases[0]); i++) {
    plat_config_t cfg = *base_cfg;
    cfg.requested_color_mode = PLAT_COLOR_MODE_UNKNOWN;
    if (zr_expect_color_mode(kCases[i].env_case.name, &kCases[i].env_case, &cfg, kCases[i].expected_mode) != 0) {
      return -1;
    }
  }
  return 0;
}

static int zr_run_request_clamp_matrix(const plat_config_t* base_cfg) {
  static const plat_color_mode_t kRequestedModes[ZR_COLOR_REQUEST_COUNT] = {PLAT_COLOR_MODE_UNKNOWN, PLAT_COLOR_MODE_16,
                                                                            PLAT_COLOR_MODE_256, PLAT_COLOR_MODE_RGB};
  static const struct {
    zr_color_env_case_t env_case;
    plat_color_mode_t expected[ZR_COLOR_REQUEST_COUNT];
  } kCases[] = {
      {.env_case = {.name = "clamp-detected-16-linux", .term = "linux"},
       .expected = {PLAT_COLOR_MODE_16, PLAT_COLOR_MODE_16, PLAT_COLOR_MODE_16, PLAT_COLOR_MODE_16}},
      {.env_case = {.name = "clamp-detected-256-xterm", .term = "xterm-256color"},
       .expected = {PLAT_COLOR_MODE_256, PLAT_COLOR_MODE_16, PLAT_COLOR_MODE_256, PLAT_COLOR_MODE_256}},
      {.env_case = {.name = "clamp-detected-rgb-colorterm", .term = "xterm-256color", .colorterm = "truecolor"},
       .expected = {PLAT_COLOR_MODE_RGB, PLAT_COLOR_MODE_16, PLAT_COLOR_MODE_256, PLAT_COLOR_MODE_RGB}},
      {.env_case = {.name = "clamp-dumb-term-beats-rgb-hints", .term = "dumb", .colorterm = "truecolor"},
       .expected = {PLAT_COLOR_MODE_16, PLAT_COLOR_MODE_16, PLAT_COLOR_MODE_16, PLAT_COLOR_MODE_16}},
  };

  for (size_t case_index = 0u; case_index < sizeof(kCases) / sizeof(kCases[0]); case_index++) {
    for (size_t req_index = 0u; req_index < ZR_COLOR_REQUEST_COUNT; req_index++) {
      char label[128];
      int label_n = snprintf(label, sizeof(label), "%s/request=%u", kCases[case_index].env_case.name,
                             (unsigned)kRequestedModes[req_index]);
      if (label_n <= 0 || (size_t)label_n >= sizeof(label)) {
        fprintf(stderr, "snprintf() failed while building clamp label\n");
        return -1;
      }

      plat_config_t cfg = *base_cfg;
      cfg.requested_color_mode = kRequestedModes[req_index];
      if (zr_expect_color_mode(label, &kCases[case_index].env_case, &cfg, kCases[case_index].expected[req_index]) !=
          0) {
        return -1;
      }
    }
  }
  return 0;
}

int main(void) {
  int rc = 2;
  int master_fd = -1;
  int slave_fd = -1;
  plat_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.requested_color_mode = PLAT_COLOR_MODE_UNKNOWN;
  cfg.enable_mouse = 0u;
  cfg.enable_bracketed_paste = 0u;
  cfg.enable_focus_events = 0u;
  cfg.enable_osc52 = 0u;

  static const zr_color_env_case_t kBaselineEnv = {
      .name = "baseline-xterm-256color",
      .term = "xterm-256color",
  };

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

  if (zr_set_cap_override_defaults() != 0) {
    goto cleanup;
  }

  if (zr_apply_color_env_case(&kBaselineEnv) != 0) {
    goto cleanup;
  }
  plat_caps_t caps;
  if (zr_read_caps_for_cfg(&cfg, "baseline", &caps) != 0) {
    goto cleanup;
  }

  if (caps.color_mode != PLAT_COLOR_MODE_256) {
    fprintf(stderr, "baseline color_mode mismatch: got=%u want=%u\n", (unsigned)caps.color_mode,
            (unsigned)PLAT_COLOR_MODE_256);
    goto cleanup;
  }

  if (caps.supports_mouse != 0u || caps.supports_bracketed_paste != 0u || caps.supports_osc52 != 0u ||
      caps.supports_sync_update != 1u || caps.supports_scroll_region != 0u || caps.supports_cursor_shape != 0u ||
      caps.supports_focus_events != 0u) {
    fprintf(stderr, "override mismatch: mouse=%u paste=%u focus=%u osc52=%u sync=%u scroll=%u cursor=%u\n",
            (unsigned)caps.supports_mouse, (unsigned)caps.supports_bracketed_paste,
            (unsigned)caps.supports_focus_events, (unsigned)caps.supports_osc52, (unsigned)caps.supports_sync_update,
            (unsigned)caps.supports_scroll_region, (unsigned)caps.supports_cursor_shape);
    goto cleanup;
  }

  if (caps.sgr_attrs_supported != (uint32_t)(ZR_STYLE_ATTR_BASIC_MASK | ZR_STYLE_ATTR_ITALIC | ZR_STYLE_ATTR_STRIKE)) {
    fprintf(stderr, "baseline sgr attrs mismatch: got=0x%08X want=0x%08X\n", (unsigned)caps.sgr_attrs_supported,
            (unsigned)(ZR_STYLE_ATTR_BASIC_MASK | ZR_STYLE_ATTR_ITALIC | ZR_STYLE_ATTR_STRIKE));
    goto cleanup;
  }

  if (zr_run_color_detection_matrix(&cfg) != 0) {
    goto cleanup;
  }

  if (zr_apply_color_env_case(&(zr_color_env_case_t){.name = "linux-term-basic-sgr", .term = "linux"}) != 0) {
    goto cleanup;
  }
  if (zr_read_caps_for_cfg(&cfg, "linux-term-sgr", &caps) != 0) {
    goto cleanup;
  }
  if (caps.sgr_attrs_supported != (uint32_t)ZR_STYLE_ATTR_BASIC_MASK) {
    fprintf(stderr, "linux term sgr attrs mismatch: got=0x%08X want=0x%08X\n", (unsigned)caps.sgr_attrs_supported,
            (unsigned)ZR_STYLE_ATTR_BASIC_MASK);
    goto cleanup;
  }

  if (setenv("ZIREAEL_CAP_SGR_ATTRS_MASK", "0x3", 1) != 0) {
    fprintf(stderr, "setenv(ZIREAEL_CAP_SGR_ATTRS_MASK) failed: errno=%d\n", errno);
    goto cleanup;
  }
  if (zr_apply_color_env_case(&kBaselineEnv) != 0) {
    goto cleanup;
  }
  if (zr_read_caps_for_cfg(&cfg, "sgr-attr-mask-override", &caps) != 0) {
    goto cleanup;
  }
  if (caps.sgr_attrs_supported != 0x3u) {
    fprintf(stderr, "sgr attrs override mismatch: got=0x%08X want=0x00000003\n", (unsigned)caps.sgr_attrs_supported);
    goto cleanup;
  }
  (void)unsetenv("ZIREAEL_CAP_SGR_ATTRS_MASK");

  if (setenv("ZIREAEL_CAP_SGR_ATTRS_MASK", "-1", 1) != 0) {
    fprintf(stderr, "setenv(ZIREAEL_CAP_SGR_ATTRS_MASK=-1) failed: errno=%d\n", errno);
    goto cleanup;
  }
  if (zr_apply_color_env_case(&(zr_color_env_case_t){.name = "linux-term-sgr-negative-mask", .term = "linux"}) != 0) {
    goto cleanup;
  }
  if (zr_read_caps_for_cfg(&cfg, "sgr-attr-mask-negative-rejected", &caps) != 0) {
    goto cleanup;
  }
  if (caps.sgr_attrs_supported != (uint32_t)ZR_STYLE_ATTR_BASIC_MASK) {
    fprintf(stderr, "negative sgr attrs override should be ignored: got=0x%08X want=0x%08X\n",
            (unsigned)caps.sgr_attrs_supported, (unsigned)ZR_STYLE_ATTR_BASIC_MASK);
    goto cleanup;
  }
  (void)unsetenv("ZIREAEL_CAP_SGR_ATTRS_MASK");

  if (zr_run_request_clamp_matrix(&cfg) != 0) {
    goto cleanup;
  }

  rc = 0;

cleanup:
  zr_clear_cap_override_env();
  (void)zr_clear_color_detection_env();
  if (slave_fd >= 0) {
    (void)close(slave_fd);
  }
  if (master_fd >= 0) {
    (void)close(master_fd);
  }
  return rc;
}
