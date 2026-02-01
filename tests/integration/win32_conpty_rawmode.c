/*
  tests/integration/win32_conpty_rawmode.c — ConPTY-based raw-mode enter/leave VT sequences (Win32 backend).

  Why: Validates deterministic VT sequence ordering and idempotent leave behavior
  for the Win32 platform backend without requiring an interactive console.
*/

#include "platform/zr_platform.h"
#include "platform/win32/zr_win32_conpty_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int zr_test_skip(const char* reason) {
  fprintf(stdout, "SKIP: %s\n", reason);
  return 77;
}

static const uint8_t* zr_memmem(const uint8_t* hay, size_t hay_len, const uint8_t* needle, size_t needle_len) {
  if (!hay || !needle) {
    return NULL;
  }
  if (needle_len == 0u) {
    return hay;
  }
  if (hay_len < needle_len) {
    return NULL;
  }

  for (size_t i = 0; i + needle_len <= hay_len; i++) {
    if (memcmp(hay + i, needle, needle_len) == 0) {
      return hay + i;
    }
  }
  return NULL;
}

static void zr_dump_hex_prefix(const uint8_t* bytes, size_t len, size_t max) {
  if (!bytes) {
    return;
  }
  if (len > max) {
    len = max;
  }
  for (size_t i = 0; i < len; i++) {
    fprintf(stderr, "%02X", (unsigned)bytes[i]);
    if ((i + 1u) % 16u == 0u) {
      fprintf(stderr, "\n");
    } else {
      fprintf(stderr, " ");
    }
  }
  if (len % 16u != 0u) {
    fprintf(stderr, "\n");
  }
}

static int zr_child_main(void) {
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
    return 2;
  }

  r = plat_enter_raw(plat);
  if (r != ZR_OK) {
    plat_destroy(plat);
    return 2;
  }

  r = plat_leave_raw(plat);
  if (r != ZR_OK) {
    plat_destroy(plat);
    return 2;
  }

  /* Idempotence: second leave must be safe and return OK. */
  r = plat_leave_raw(plat);
  if (r != ZR_OK) {
    plat_destroy(plat);
    return 2;
  }

  plat_destroy(plat);
  return 0;
}

int main(int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "--child") == 0) {
    return zr_child_main();
  }

  uint8_t out[4096];
  memset(out, 0, sizeof(out));
  size_t out_len = 0u;
  uint32_t exit_code = 0u;
  char skip_reason[256];
  memset(skip_reason, 0, sizeof(skip_reason));

  zr_result_t r = zr_win32_conpty_run_self_capture("--child", out, sizeof(out), &out_len, &exit_code, skip_reason, sizeof(skip_reason));
  if (r == ZR_ERR_UNSUPPORTED) {
    return zr_test_skip(skip_reason[0] ? skip_reason : "ConPTY unavailable");
  }
  if (r != ZR_OK) {
    fprintf(stderr, "ConPTY runner failed: r=%d\n", (int)r);
    return 2;
  }
  if (exit_code != 0u) {
    fprintf(stderr, "child failed: exit_code=%u\n", (unsigned)exit_code);
    return 2;
  }

  static const uint8_t tok_alt_enter[] = "\x1b[?1049h";
  static const uint8_t tok_cursor_hide[] = "\x1b[?25l";
  static const uint8_t tok_bp_enter[] = "\x1b[?2004h";
  static const uint8_t tok_mouse_1000_enter[] = "\x1b[?1000h";
  static const uint8_t tok_mouse_1006_enter[] = "\x1b[?1006h";

  static const uint8_t tok_mouse_1006_leave[] = "\x1b[?1006l";
  static const uint8_t tok_mouse_1000_leave[] = "\x1b[?1000l";
  static const uint8_t tok_bp_leave[] = "\x1b[?2004l";
  static const uint8_t tok_alt_leave[] = "\x1b[?1049l";

  /*
    ConPTY output can contain unavoidable initialization noise and may normalize
    state transitions. Validate the backend-emitted sequences as an ordered
    subsequence.
  */
  const uint8_t* p = out;
  size_t remaining = out_len;

  const uint8_t* pos_alt_enter = zr_memmem(p, remaining, tok_alt_enter, sizeof(tok_alt_enter) - 1u);
  if (!pos_alt_enter) {
    fprintf(stderr, "alt-screen enter token not found (len=%zu)\n", out_len);
    zr_dump_hex_prefix(out, out_len, 512u);
    return 2;
  }
  p = pos_alt_enter + (sizeof(tok_alt_enter) - 1u);
  remaining = out_len - (size_t)(p - out);

  const uint8_t* pos_cursor_hide = zr_memmem(p, remaining, tok_cursor_hide, sizeof(tok_cursor_hide) - 1u);
  if (!pos_cursor_hide) {
    fprintf(stderr, "cursor-hide token not found after alt-screen enter\n");
    zr_dump_hex_prefix(out, out_len, 512u);
    return 2;
  }
  p = pos_cursor_hide + (sizeof(tok_cursor_hide) - 1u);
  remaining = out_len - (size_t)(p - out);

  const uint8_t* pos_bp_enter = zr_memmem(p, remaining, tok_bp_enter, sizeof(tok_bp_enter) - 1u);
  if (!pos_bp_enter) {
    fprintf(stderr, "bracketed-paste enable token not found after cursor-hide\n");
    zr_dump_hex_prefix(out, out_len, 512u);
    return 2;
  }
  p = pos_bp_enter + (sizeof(tok_bp_enter) - 1u);
  remaining = out_len - (size_t)(p - out);

  const uint8_t* pos_mouse_1000 = zr_memmem(p, remaining, tok_mouse_1000_enter, sizeof(tok_mouse_1000_enter) - 1u);
  if (!pos_mouse_1000) {
    fprintf(stderr, "mouse ?1000h token not found after bracketed-paste enable\n");
    zr_dump_hex_prefix(out, out_len, 512u);
    return 2;
  }
  p = pos_mouse_1000 + (sizeof(tok_mouse_1000_enter) - 1u);
  remaining = out_len - (size_t)(p - out);

  const uint8_t* pos_mouse_1006 = zr_memmem(p, remaining, tok_mouse_1006_enter, sizeof(tok_mouse_1006_enter) - 1u);
  if (!pos_mouse_1006) {
    fprintf(stderr, "mouse ?1006h token not found after mouse ?1000h\n");
    zr_dump_hex_prefix(out, out_len, 512u);
    return 2;
  }
  p = pos_mouse_1006 + (sizeof(tok_mouse_1006_enter) - 1u);
  remaining = out_len - (size_t)(p - out);

  const uint8_t* pos_mouse_1006_l = zr_memmem(p, remaining, tok_mouse_1006_leave, sizeof(tok_mouse_1006_leave) - 1u);
  if (!pos_mouse_1006_l) {
    fprintf(stderr, "mouse ?1006l token not found after enter tokens\n");
    zr_dump_hex_prefix(out, out_len, 512u);
    return 2;
  }
  p = pos_mouse_1006_l + (sizeof(tok_mouse_1006_leave) - 1u);
  remaining = out_len - (size_t)(p - out);

  const uint8_t* pos_mouse_1000_l = zr_memmem(p, remaining, tok_mouse_1000_leave, sizeof(tok_mouse_1000_leave) - 1u);
  if (!pos_mouse_1000_l) {
    fprintf(stderr, "mouse ?1000l token not found after mouse ?1006l\n");
    zr_dump_hex_prefix(out, out_len, 512u);
    return 2;
  }
  p = pos_mouse_1000_l + (sizeof(tok_mouse_1000_leave) - 1u);
  remaining = out_len - (size_t)(p - out);

  const uint8_t* pos_bp_leave = zr_memmem(p, remaining, tok_bp_leave, sizeof(tok_bp_leave) - 1u);
  if (!pos_bp_leave) {
    fprintf(stderr, "bracketed-paste disable token not found after mouse disable\n");
    zr_dump_hex_prefix(out, out_len, 512u);
    return 2;
  }
  p = pos_bp_leave + (sizeof(tok_bp_leave) - 1u);
  remaining = out_len - (size_t)(p - out);

  const uint8_t* pos_alt_leave = zr_memmem(p, remaining, tok_alt_leave, sizeof(tok_alt_leave) - 1u);
  if (!pos_alt_leave) {
    fprintf(stderr, "alt-screen leave token not found after bracketed-paste disable\n");
    zr_dump_hex_prefix(out, out_len, 512u);
    return 2;
  }

  return 0;
}
