/*
  tests/integration/win32_caps_overrides.c — Win32 capability override contract under ConPTY.

  Why: Pins deterministic capability behavior for the Win32 backend: color-mode
  clamp policy, focus/output-writable override paths, and SGR attr-mask parsing.
*/

#if !defined(_WIN32)

#include <stdio.h>

int main(void) {
  fprintf(stdout, "SKIP: win32-only integration test\n");
  return 77;
}

#else

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "platform/zr_platform.h"
#include "platform/win32/zr_win32_conpty_test.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum {
  ZR_STYLE_ATTR_BOLD = 1u << 0u,
  ZR_STYLE_ATTR_ITALIC = 1u << 1u,
  ZR_STYLE_ATTR_UNDERLINE = 1u << 2u,
  ZR_STYLE_ATTR_REVERSE = 1u << 3u,
  ZR_STYLE_ATTR_DIM = 1u << 4u,
  ZR_STYLE_ATTR_STRIKE = 1u << 5u,
  ZR_STYLE_ATTR_BASIC_MASK = ZR_STYLE_ATTR_BOLD | ZR_STYLE_ATTR_UNDERLINE | ZR_STYLE_ATTR_REVERSE | ZR_STYLE_ATTR_DIM,
  ZR_STYLE_ATTR_ALL_MASK = (1u << 6u) - 1u,
};

typedef struct zr_host_env_case_t {
  const char* name;
  const char* term;
  const char* term_program;
  const char* wt_session;
  const char* kitty_window_id;
  const char* wezterm_pane;
  const char* wezterm_executable;
  const char* ansicon;
  const char* conemu_ansi;
} zr_host_env_case_t;

static int zr_test_skip(const char* reason) {
  fprintf(stdout, "SKIP: %s\n", reason);
  return 77;
}

static void zr_dump_child_output(const uint8_t* out, size_t out_len) {
  if (!out || out_len == 0u) {
    return;
  }
  fprintf(stderr, "child output (%zu bytes):\n", out_len);
  (void)fwrite(out, 1u, out_len, stderr);
  if (out[out_len - 1u] != (uint8_t)'\n') {
    (void)fputc('\n', stderr);
  }
}

static int zr_env_set_optional(const char* key, const char* value) {
  if (!key) {
    return -1;
  }

  if (value) {
    if (_putenv_s(key, value) != 0) {
      fprintf(stderr, "_putenv_s(%s, %s) failed\n", key, value);
      return -1;
    }
  } else {
    if (_putenv_s(key, "") != 0) {
      fprintf(stderr, "_putenv_s(%s, <unset>) failed\n", key);
      return -1;
    }
  }

  if (SetEnvironmentVariableA(key, value) == 0) {
    const DWORD gle = GetLastError();
    if (!value && gle == ERROR_ENVVAR_NOT_FOUND) {
      return 0;
    }
    fprintf(stderr, "SetEnvironmentVariableA(%s, %s) failed: gle=%lu\n", key, value ? value : "<null>",
            (unsigned long)gle);
    return -1;
  }

  return 0;
}

/* Clear env markers used by Win32 modern-host/focus/SGR detection. */
static int zr_clear_host_detection_env(void) {
  static const char* kKeys[] = {
      "TERM",         "TERM_PROGRAM",       "WT_SESSION", "KITTY_WINDOW_ID",
      "WEZTERM_PANE", "WEZTERM_EXECUTABLE", "ANSICON",    "ConEmuANSI",
  };

  for (size_t i = 0u; i < sizeof(kKeys) / sizeof(kKeys[0]); i++) {
    if (zr_env_set_optional(kKeys[i], NULL) != 0) {
      return -1;
    }
  }
  return 0;
}

static void zr_clear_cap_override_env(void) {
  (void)SetEnvironmentVariableA("ZIREAEL_CAP_MOUSE", NULL);
  (void)SetEnvironmentVariableA("ZIREAEL_CAP_BRACKETED_PASTE", NULL);
  (void)SetEnvironmentVariableA("ZIREAEL_CAP_OSC52", NULL);
  (void)SetEnvironmentVariableA("ZIREAEL_CAP_SYNC_UPDATE", NULL);
  (void)SetEnvironmentVariableA("ZIREAEL_CAP_SCROLL_REGION", NULL);
  (void)SetEnvironmentVariableA("ZIREAEL_CAP_CURSOR_SHAPE", NULL);
  (void)SetEnvironmentVariableA("ZIREAEL_CAP_OUTPUT_WAIT_WRITABLE", NULL);
  (void)SetEnvironmentVariableA("ZIREAEL_CAP_FOCUS_EVENTS", NULL);
  (void)SetEnvironmentVariableA("ZIREAEL_CAP_SGR_ATTRS", NULL);
  (void)SetEnvironmentVariableA("ZIREAEL_CAP_SGR_ATTRS_MASK", NULL);
}

/* Apply one deterministic env case by resetting all relevant detection keys first. */
static int zr_apply_host_env_case(const zr_host_env_case_t* env_case) {
  if (!env_case) {
    return -1;
  }

  if (zr_clear_host_detection_env() != 0) {
    return -1;
  }

  if (zr_env_set_optional("TERM", env_case->term) != 0 ||
      zr_env_set_optional("TERM_PROGRAM", env_case->term_program) != 0 ||
      zr_env_set_optional("WT_SESSION", env_case->wt_session) != 0 ||
      zr_env_set_optional("KITTY_WINDOW_ID", env_case->kitty_window_id) != 0 ||
      zr_env_set_optional("WEZTERM_PANE", env_case->wezterm_pane) != 0 ||
      zr_env_set_optional("WEZTERM_EXECUTABLE", env_case->wezterm_executable) != 0 ||
      zr_env_set_optional("ANSICON", env_case->ansicon) != 0 ||
      zr_env_set_optional("ConEmuANSI", env_case->conemu_ansi) != 0) {
    fprintf(stderr, "failed to apply host env case '%s'\n", env_case->name ? env_case->name : "unnamed");
    return -1;
  }

  return 0;
}

/* Create platform, read caps, then destroy to keep each assertion isolated. */
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

static int zr_wait_output_rc_is_supported(zr_result_t r) {
  return (r == ZR_OK || r == ZR_ERR_LIMIT) ? 1 : 0;
}

static int zr_set_temp_stdout(HANDLE* out_saved_stdout, HANDLE new_stdout, const char* label) {
  if (!out_saved_stdout || !label || !new_stdout || new_stdout == INVALID_HANDLE_VALUE) {
    return -1;
  }

  HANDLE saved_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
  if (!saved_stdout || saved_stdout == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "GetStdHandle(STD_OUTPUT_HANDLE) failed (%s)\n", label);
    return -1;
  }
  if (!SetStdHandle(STD_OUTPUT_HANDLE, new_stdout)) {
    fprintf(stderr, "SetStdHandle(STD_OUTPUT_HANDLE, new) failed (%s): gle=%lu\n", label,
            (unsigned long)GetLastError());
    return -1;
  }
  *out_saved_stdout = saved_stdout;
  return 0;
}

static int zr_restore_stdout(HANDLE saved_stdout, const char* label) {
  if (!saved_stdout || saved_stdout == INVALID_HANDLE_VALUE || !label) {
    return -1;
  }
  if (!SetStdHandle(STD_OUTPUT_HANDLE, saved_stdout)) {
    fprintf(stderr, "SetStdHandle(STD_OUTPUT_HANDLE, restore) failed (%s): gle=%lu\n", label,
            (unsigned long)GetLastError());
    return -1;
  }
  return 0;
}

/* Validate cap + runtime output wait behavior against the current active STDOUT handle. */
static int zr_expect_output_wait_for_active_stdout(const plat_config_t* cfg, const char* label, uint8_t expected_cap,
                                                   int expect_wait_supported) {
  if (!cfg || !label) {
    return -1;
  }

  int rc = -1;
  plat_t* plat = NULL;
  zr_result_t r = plat_create(&plat, cfg);
  if (r != ZR_OK || !plat) {
    fprintf(stderr, "plat_create() failed (%s): r=%d\n", label, (int)r);
    return -1;
  }

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  r = plat_get_caps(plat, &caps);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_get_caps() failed (%s): r=%d\n", label, (int)r);
    rc = -1;
    goto destroy_plat;
  }
  if (caps.supports_output_wait_writable != expected_cap) {
    fprintf(stderr, "output-writable cap mismatch (%s): got=%u want=%u\n", label,
            (unsigned)caps.supports_output_wait_writable, (unsigned)expected_cap);
    rc = -1;
    goto destroy_plat;
  }

  r = plat_wait_output_writable(plat, 0);
  if (expect_wait_supported) {
    if (!zr_wait_output_rc_is_supported(r)) {
      fprintf(stderr, "plat_wait_output_writable() should be supported (%s): r=%d\n", label, (int)r);
      goto destroy_plat;
    }
  } else if (r != ZR_ERR_UNSUPPORTED) {
    fprintf(stderr, "plat_wait_output_writable() should be unsupported (%s): r=%d\n", label, (int)r);
    goto destroy_plat;
  }

  rc = 0;

destroy_plat:
  plat_destroy(plat);
  return rc;
}

/*
  Swap STDOUT for one handle type, then validate cap + runtime wait behavior.

  Why: Win32 may expose stdout as console char, pipe (ConPTY), or disk/file.
  This helper pins deterministic behavior across those handle classes.
*/
static int zr_expect_output_wait_for_stdout_handle(const plat_config_t* cfg, const char* label, HANDLE new_stdout,
                                                   uint8_t expected_cap, int expect_wait_supported) {
  HANDLE saved_stdout = NULL;
  if (zr_set_temp_stdout(&saved_stdout, new_stdout, label) != 0) {
    return -1;
  }

  const int rc = zr_expect_output_wait_for_active_stdout(cfg, label, expected_cap, expect_wait_supported);
  if (zr_restore_stdout(saved_stdout, label) != 0) {
    return -1;
  }
  return rc;
}

static HANDLE zr_open_temp_output_file(char temp_name[MAX_PATH]) {
  if (!temp_name) {
    return INVALID_HANDLE_VALUE;
  }
  temp_name[0] = '\0';
  if (GetTempFileNameA(".", "zrw", 0u, temp_name) == 0u) {
    fprintf(stderr, "GetTempFileNameA() failed: gle=%lu\n", (unsigned long)GetLastError());
    return INVALID_HANDLE_VALUE;
  }

  HANDLE h_file =
      CreateFileA(temp_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                  OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
  if (!h_file || h_file == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "CreateFileA(temp) failed: gle=%lu\n", (unsigned long)GetLastError());
    (void)DeleteFileA(temp_name);
    temp_name[0] = '\0';
    return INVALID_HANDLE_VALUE;
  }
  return h_file;
}

static int zr_check_output_wait_char_case(const plat_config_t* base_cfg) {
  HANDLE h_nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0u, NULL);
  if (!h_nul || h_nul == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "CreateFileA(NUL) failed: gle=%lu\n", (unsigned long)GetLastError());
    return -1;
  }
  const int rc = zr_expect_output_wait_for_stdout_handle(base_cfg, "output-writable-handle-char", h_nul, 1u, 1);
  (void)CloseHandle(h_nul);
  return rc;
}

static int zr_check_output_wait_pipe_case(const plat_config_t* base_cfg) {
  HANDLE h_pipe_read = NULL;
  HANDLE h_pipe_write = NULL;
  if (!CreatePipe(&h_pipe_read, &h_pipe_write, NULL, 0u)) {
    fprintf(stderr, "CreatePipe() failed: gle=%lu\n", (unsigned long)GetLastError());
    return -1;
  }
  const int rc = zr_expect_output_wait_for_stdout_handle(base_cfg, "output-writable-handle-pipe", h_pipe_write, 1u, 1);
  (void)CloseHandle(h_pipe_write);
  (void)CloseHandle(h_pipe_read);
  return rc;
}

static int zr_check_output_wait_disk_case(const plat_config_t* base_cfg) {
  char temp_name[MAX_PATH];
  HANDLE h_file = zr_open_temp_output_file(temp_name);
  if (!h_file || h_file == INVALID_HANDLE_VALUE) {
    return -1;
  }
  const int rc = zr_expect_output_wait_for_stdout_handle(base_cfg, "output-writable-handle-disk", h_file, 1u, 1);
  (void)CloseHandle(h_file);
  if (temp_name[0] != '\0') {
    (void)DeleteFileA(temp_name);
  }
  return rc;
}

/*
  Validate output-writable parity across common Win32 stdout handle classes.

  Why: Engine config gates wait-for-output-drain on this cap. False unsupported
  states on safe handle types break parity with POSIX and ConPTY environments.
*/
static int zr_run_output_writable_handle_type_checks(const plat_config_t* base_cfg) {
  if (!base_cfg) {
    return -1;
  }
  if (zr_env_set_optional("ZIREAEL_CAP_OUTPUT_WAIT_WRITABLE", NULL) != 0) {
    return -1;
  }
  if (zr_check_output_wait_char_case(base_cfg) != 0) {
    return -1;
  }
  if (zr_check_output_wait_pipe_case(base_cfg) != 0) {
    return -1;
  }
  if (zr_check_output_wait_disk_case(base_cfg) != 0) {
    return -1;
  }
  return 0;
}

/* Validate Win32 color clamp behavior: requested mode cannot exceed detected RGB. */
static int zr_run_color_clamp_matrix(const plat_config_t* base_cfg) {
  static const struct {
    plat_color_mode_t requested;
    plat_color_mode_t expected;
  } kCases[] = {
      {PLAT_COLOR_MODE_UNKNOWN, PLAT_COLOR_MODE_RGB},
      {PLAT_COLOR_MODE_16, PLAT_COLOR_MODE_16},
      {PLAT_COLOR_MODE_256, PLAT_COLOR_MODE_256},
      {PLAT_COLOR_MODE_RGB, PLAT_COLOR_MODE_RGB},
  };

  for (size_t i = 0u; i < sizeof(kCases) / sizeof(kCases[0]); i++) {
    char label[96];
    int n = snprintf(label, sizeof(label), "color-clamp/request=%u", (unsigned)kCases[i].requested);
    if (n <= 0 || (size_t)n >= sizeof(label)) {
      fprintf(stderr, "snprintf() failed building color clamp label\n");
      return -1;
    }

    plat_config_t cfg = *base_cfg;
    cfg.requested_color_mode = kCases[i].requested;

    plat_caps_t caps;
    if (zr_read_caps_for_cfg(&cfg, label, &caps) != 0) {
      return -1;
    }
    if (caps.color_mode != kCases[i].expected) {
      fprintf(stderr, "color clamp mismatch (%s): got=%u want=%u\n", label, (unsigned)caps.color_mode,
              (unsigned)kCases[i].expected);
      return -1;
    }
  }

  return 0;
}

/* Validate focus detection plus manual bool override handling (valid + invalid). */
static int zr_run_focus_override_checks(const plat_config_t* base_cfg) {
  static const zr_host_env_case_t kLegacyHost = {
      .name = "legacy-host-no-modern-markers",
      .term = "vt100",
  };
  static const zr_host_env_case_t kModernHost = {
      .name = "modern-host-wt-session",
      .term = "vt100",
      .wt_session = "caps-test",
  };

  if (zr_apply_host_env_case(&kLegacyHost) != 0) {
    return -1;
  }
  plat_caps_t caps;
  if (zr_read_caps_for_cfg(base_cfg, "focus-legacy-baseline", &caps) != 0) {
    return -1;
  }
  if (caps.supports_focus_events != 0u) {
    fprintf(stderr, "focus baseline mismatch: got=%u want=0\n", (unsigned)caps.supports_focus_events);
    return -1;
  }

  if (zr_apply_host_env_case(&kModernHost) != 0) {
    return -1;
  }
  if (zr_read_caps_for_cfg(base_cfg, "focus-modern-baseline", &caps) != 0) {
    return -1;
  }
  if (caps.supports_focus_events != 1u) {
    fprintf(stderr, "focus modern mismatch: got=%u want=1\n", (unsigned)caps.supports_focus_events);
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_FOCUS_EVENTS", "0") != 0) {
    return -1;
  }
  if (zr_read_caps_for_cfg(base_cfg, "focus-manual-off", &caps) != 0) {
    return -1;
  }
  if (caps.supports_focus_events != 0u) {
    fprintf(stderr, "focus manual override mismatch: got=%u want=0\n", (unsigned)caps.supports_focus_events);
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_FOCUS_EVENTS", "not-a-bool") != 0) {
    return -1;
  }
  if (zr_read_caps_for_cfg(base_cfg, "focus-invalid-ignored", &caps) != 0) {
    return -1;
  }
  if (caps.supports_focus_events != 1u) {
    fprintf(stderr, "focus invalid override should be ignored: got=%u want=1\n", (unsigned)caps.supports_focus_events);
    return -1;
  }

  (void)zr_env_set_optional("ZIREAEL_CAP_FOCUS_EVENTS", NULL);
  return 0;
}

/* Validate output-writable override behavior, including deterministic unsupported path. */
static int zr_run_output_writable_override_checks(const plat_config_t* base_cfg) {
  static const zr_host_env_case_t kHost = {
      .name = "output-writable-baseline-host",
      .term = "vt100",
  };

  if (zr_apply_host_env_case(&kHost) != 0) {
    return -1;
  }
  if (zr_run_output_writable_handle_type_checks(base_cfg) != 0) {
    return -1;
  }

  plat_caps_t caps_baseline;
  if (zr_read_caps_for_cfg(base_cfg, "output-writable-baseline", &caps_baseline) != 0) {
    return -1;
  }
  if (caps_baseline.supports_output_wait_writable != 1u) {
    fprintf(stderr, "output-writable baseline mismatch: got=%u want=1\n",
            (unsigned)caps_baseline.supports_output_wait_writable);
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_OUTPUT_WAIT_WRITABLE", "not-a-bool") != 0) {
    return -1;
  }
  plat_caps_t caps_invalid;
  if (zr_read_caps_for_cfg(base_cfg, "output-writable-invalid-ignored", &caps_invalid) != 0) {
    return -1;
  }
  if (caps_invalid.supports_output_wait_writable != caps_baseline.supports_output_wait_writable) {
    fprintf(stderr, "invalid output-writable override should be ignored: got=%u want=%u\n",
            (unsigned)caps_invalid.supports_output_wait_writable,
            (unsigned)caps_baseline.supports_output_wait_writable);
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_OUTPUT_WAIT_WRITABLE", "1") != 0) {
    return -1;
  }
  plat_caps_t caps_on;
  if (zr_read_caps_for_cfg(base_cfg, "output-writable-manual-on", &caps_on) != 0) {
    return -1;
  }
  if (caps_on.supports_output_wait_writable != 1u) {
    fprintf(stderr, "output-writable manual on mismatch: got=%u want=1\n",
            (unsigned)caps_on.supports_output_wait_writable);
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_OUTPUT_WAIT_WRITABLE", "0") != 0) {
    return -1;
  }

  plat_t* plat = NULL;
  zr_result_t r = plat_create(&plat, base_cfg);
  if (r != ZR_OK || !plat) {
    fprintf(stderr, "plat_create() failed (output-writable-manual-off): r=%d\n", (int)r);
    return -1;
  }

  plat_caps_t caps;
  r = plat_get_caps(plat, &caps);
  if (r != ZR_OK) {
    fprintf(stderr, "plat_get_caps() failed (output-writable-manual-off): r=%d\n", (int)r);
    plat_destroy(plat);
    return -1;
  }
  if (caps.supports_output_wait_writable != 0u) {
    fprintf(stderr, "output-writable manual off mismatch: got=%u want=0\n",
            (unsigned)caps.supports_output_wait_writable);
    plat_destroy(plat);
    return -1;
  }

  r = plat_wait_output_writable(plat, 0);
  plat_destroy(plat);
  if (r != ZR_ERR_UNSUPPORTED) {
    fprintf(stderr, "plat_wait_output_writable() with manual off should be unsupported: r=%d\n", (int)r);
    return -1;
  }

  (void)zr_env_set_optional("ZIREAEL_CAP_OUTPUT_WAIT_WRITABLE", NULL);
  return 0;
}

/* Read SGR mask from caps and compare with expected value for one test case. */
static int zr_expect_sgr_attrs(const plat_config_t* cfg, const char* label, uint32_t expected_attrs) {
  plat_caps_t caps;
  if (zr_read_caps_for_cfg(cfg, label, &caps) != 0) {
    return -1;
  }
  if (caps.sgr_attrs_supported != expected_attrs) {
    fprintf(stderr, "sgr attrs mismatch (%s): got=0x%08X want=0x%08X\n", label ? label : "n/a",
            (unsigned)caps.sgr_attrs_supported, (unsigned)expected_attrs);
    return -1;
  }
  return 0;
}

/* Validate SGR u32 override parsing: valid, invalid, signed-invalid, and precedence. */
static int zr_run_sgr_override_matrix(const plat_config_t* base_cfg) {
  static const zr_host_env_case_t kLegacyHost = {
      .name = "sgr-legacy-host",
      .term = "vt100",
  };

  if (zr_apply_host_env_case(&kLegacyHost) != 0) {
    return -1;
  }

  zr_clear_cap_override_env();
  if (zr_expect_sgr_attrs(base_cfg, "sgr-baseline", (uint32_t)ZR_STYLE_ATTR_BASIC_MASK) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS", "0x12") != 0) {
    return -1;
  }
  if (zr_expect_sgr_attrs(base_cfg, "sgr-attrs-hex-valid", 0x12u) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS", "0xFFFFFFFF") != 0) {
    return -1;
  }
  if (zr_expect_sgr_attrs(base_cfg, "sgr-attrs-overflow-clamped-by-mask", (uint32_t)ZR_STYLE_ATTR_ALL_MASK) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS", "invalid") != 0) {
    return -1;
  }
  if (zr_expect_sgr_attrs(base_cfg, "sgr-attrs-invalid-ignored", (uint32_t)ZR_STYLE_ATTR_BASIC_MASK) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS", "-1") != 0) {
    return -1;
  }
  if (zr_expect_sgr_attrs(base_cfg, "sgr-attrs-negative-rejected", (uint32_t)ZR_STYLE_ATTR_BASIC_MASK) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS", "+1") != 0) {
    return -1;
  }
  if (zr_expect_sgr_attrs(base_cfg, "sgr-attrs-positive-signed-rejected", (uint32_t)ZR_STYLE_ATTR_BASIC_MASK) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS", NULL) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS_MASK", "0x3") != 0) {
    return -1;
  }
  if (zr_expect_sgr_attrs(base_cfg, "sgr-mask-hex-valid", 0x3u) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS_MASK", "0x20") != 0) {
    return -1;
  }
  if (zr_expect_sgr_attrs(base_cfg, "sgr-mask-strike-bit-supported", (uint32_t)ZR_STYLE_ATTR_STRIKE) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS_MASK", "-1") != 0) {
    return -1;
  }
  if (zr_expect_sgr_attrs(base_cfg, "sgr-mask-negative-rejected", (uint32_t)ZR_STYLE_ATTR_BASIC_MASK) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS_MASK", "+1") != 0) {
    return -1;
  }
  if (zr_expect_sgr_attrs(base_cfg, "sgr-mask-positive-signed-rejected", (uint32_t)ZR_STYLE_ATTR_BASIC_MASK) != 0) {
    return -1;
  }

  if (zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS", "0x1F") != 0 ||
      zr_env_set_optional("ZIREAEL_CAP_SGR_ATTRS_MASK", "0x3") != 0) {
    return -1;
  }
  if (zr_expect_sgr_attrs(base_cfg, "sgr-mask-overrides-sgr-attrs", 0x3u) != 0) {
    return -1;
  }

  zr_clear_cap_override_env();
  return 0;
}

/* Child path executed inside ConPTY so plat_create has deterministic Win32 handles. */
static int zr_child_main(void) {
  int rc = 2;

  plat_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.requested_color_mode = PLAT_COLOR_MODE_UNKNOWN;
  cfg.enable_mouse = 0u;
  cfg.enable_bracketed_paste = 0u;
  cfg.enable_focus_events = 0u;
  cfg.enable_osc52 = 0u;

  if (zr_run_color_clamp_matrix(&cfg) != 0) {
    goto cleanup;
  }
  if (zr_run_focus_override_checks(&cfg) != 0) {
    goto cleanup;
  }
  if (zr_run_output_writable_override_checks(&cfg) != 0) {
    goto cleanup;
  }
  if (zr_run_sgr_override_matrix(&cfg) != 0) {
    goto cleanup;
  }

  rc = 0;

cleanup:
  zr_clear_cap_override_env();
  (void)zr_clear_host_detection_env();
  return rc;
}

int main(int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "--child") == 0) {
    return zr_child_main();
  }

  uint8_t out[1024];
  memset(out, 0, sizeof(out));
  size_t out_len = 0u;
  uint32_t exit_code = 0u;
  char skip_reason[256];
  memset(skip_reason, 0, sizeof(skip_reason));

  zr_result_t r = zr_win32_conpty_run_self_capture("--child", out, sizeof(out), &out_len, &exit_code, skip_reason,
                                                   sizeof(skip_reason));
  if (r == ZR_ERR_UNSUPPORTED) {
    return zr_test_skip(skip_reason[0] ? skip_reason : "ConPTY unavailable");
  }
  if (r != ZR_OK) {
    fprintf(stderr, "ConPTY runner failed: r=%d\n", (int)r);
    zr_dump_child_output(out, out_len);
    return 2;
  }
  if (exit_code != 0u) {
    fprintf(stderr, "child failed: exit_code=%u\n", (unsigned)exit_code);
    zr_dump_child_output(out, out_len);
    return 2;
  }

  return 0;
}

#endif
