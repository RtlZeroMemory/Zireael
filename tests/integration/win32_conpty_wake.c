/*
  tests/integration/win32_conpty_wake.c — ConPTY-based wake behavior for plat_wait/plat_wake (Win32 backend).

  Why: Ensures plat_wait is wakeable via plat_wake from a non-engine thread, and
  that the wait returns promptly without relying on interactive console input.
*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "platform/zr_platform.h"
#include "platform/win32/zr_win32_conpty_test.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int zr_test_skip(const char* reason) {
  fprintf(stdout, "SKIP: %s\n", reason);
  return 77;
}

typedef struct zr_wait_thread_args_t {
  plat_t*  plat;
  int32_t  timeout_ms;
  int32_t  result;
} zr_wait_thread_args_t;

static DWORD WINAPI zr_wait_thread(LPVOID user) {
  zr_wait_thread_args_t* args = (zr_wait_thread_args_t*)user;
  args->result = plat_wait(args->plat, args->timeout_ms);
  return 0u;
}

static DWORD WINAPI zr_wake_thread(LPVOID user) {
  plat_t* plat = (plat_t*)user;
  (void)plat_wake(plat);
  return 0u;
}

static int zr_child_main(void) {
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
    return 2;
  }

  zr_wait_thread_args_t wait_args;
  memset(&wait_args, 0, sizeof(wait_args));
  wait_args.plat = plat;
  wait_args.timeout_ms = 5000;
  wait_args.result = -999;

  HANDLE th_wait = CreateThread(NULL, 0u, zr_wait_thread, &wait_args, 0u, NULL);
  if (!th_wait) {
    plat_destroy(plat);
    return 2;
  }

  Sleep(50u);
  HANDLE th_wake = CreateThread(NULL, 0u, zr_wake_thread, plat, 0u, NULL);
  if (!th_wake) {
    (void)WaitForSingleObject(th_wait, 1000u);
    CloseHandle(th_wait);
    plat_destroy(plat);
    return 2;
  }

  DWORD wait_join = WaitForSingleObject(th_wait, 3000u);
  DWORD wake_join = WaitForSingleObject(th_wake, 3000u);
  CloseHandle(th_wait);
  CloseHandle(th_wake);

  if (wait_join != WAIT_OBJECT_0 || wake_join != WAIT_OBJECT_0) {
    plat_destroy(plat);
    return 2;
  }

  if (wait_args.result != 1) {
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

  uint8_t out[1024];
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

  return 0;
}
