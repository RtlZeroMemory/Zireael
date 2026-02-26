/*
  tests/integration/win32_conpty_engine_post_user_event.c — Engine-level cross-thread user-event wake test (Win32).

  Why: Verifies that engine_post_user_event() can wake a blocked
  engine_poll_events() call running on another thread and that user payload bytes
  survive packed-event serialization.
*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "zr/zr_engine.h"
#include "zr/zr_event.h"
#include "zr/zr_version.h"

#include "platform/win32/zr_win32_conpty_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int zr_test_skip(const char* reason) {
  fprintf(stdout, "SKIP: %s\n", reason);
  return 77;
}

typedef struct zr_poll_thread_args_t {
  zr_engine_t* e;
  HANDLE ready_event;
  uint32_t tag;
  const uint8_t* payload;
  uint32_t payload_len;
  bool found_user;
  int n_last;
  uint8_t out[4096];
} zr_poll_thread_args_t;

static bool zr_batch_has_user_event(const uint8_t* buf, size_t len, uint32_t tag, const uint8_t* payload,
                                    uint32_t payload_len);

static DWORD WINAPI zr_poll_thread(LPVOID user) {
  enum {
    ZR_POLL_SLICE_MS = 250,
    ZR_POLL_MAX_ITERS = 20,
  };

  zr_poll_thread_args_t* args = (zr_poll_thread_args_t*)user;
  if (args->ready_event) {
    (void)SetEvent(args->ready_event);
  }
  args->found_user = false;
  args->n_last = 0;

  for (int i = 0; i < ZR_POLL_MAX_ITERS; i++) {
    const int n = engine_poll_events(args->e, ZR_POLL_SLICE_MS, args->out, (int)sizeof(args->out));
    args->n_last = n;
    if (n < 0) {
      return 0u;
    }
    if (n == 0) {
      continue;
    }
    if (zr_batch_has_user_event(args->out, (size_t)n, args->tag, args->payload, args->payload_len)) {
      args->found_user = true;
      return 0u;
    }
  }
  return 0u;
}

static bool zr_batch_has_user_event(const uint8_t* buf, size_t len, uint32_t tag, const uint8_t* payload,
                                    uint32_t payload_len) {
  if (!buf || len < sizeof(zr_evbatch_header_t)) {
    return false;
  }

  zr_evbatch_header_t batch;
  memcpy(&batch, buf, sizeof(batch));
  if (batch.magic != ZR_EV_MAGIC || batch.version != ZR_EVENT_BATCH_VERSION_V1) {
    return false;
  }

  size_t off = sizeof(zr_evbatch_header_t);
  while (off + sizeof(zr_ev_record_header_t) <= len) {
    zr_ev_record_header_t rec;
    memcpy(&rec, buf + off, sizeof(rec));

    if (rec.size < (uint32_t)sizeof(zr_ev_record_header_t) || (rec.size & 3u) != 0u) {
      return false;
    }
    if ((size_t)rec.size > (len - off)) {
      return false;
    }

    if (rec.type == (uint32_t)ZR_EV_USER) {
      const size_t payload_base = off + sizeof(zr_ev_record_header_t);
      if (payload_base + sizeof(zr_ev_user_t) > len) {
        return false;
      }

      zr_ev_user_t ev;
      memcpy(&ev, buf + payload_base, sizeof(ev));
      if (ev.tag == tag && ev.byte_len == payload_len) {
        const size_t bytes_off = payload_base + sizeof(zr_ev_user_t);
        if (bytes_off + (size_t)payload_len > len) {
          return false;
        }
        if (payload_len == 0u) {
          return true;
        }
        return memcmp(buf + bytes_off, payload, payload_len) == 0;
      }
    }

    off += (size_t)rec.size;
  }

  return false;
}

static int zr_child_main(void) {
  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.requested_engine_abi_major = ZR_ENGINE_ABI_MAJOR;
  cfg.requested_engine_abi_minor = ZR_ENGINE_ABI_MINOR;
  cfg.requested_engine_abi_patch = ZR_ENGINE_ABI_PATCH;
  cfg.requested_drawlist_version = ZR_DRAWLIST_VERSION_V6;
  cfg.requested_event_batch_version = ZR_EVENT_BATCH_VERSION_V1;
  cfg.target_fps = 0u; /* Disable periodic ZR_EV_TICK to keep wake source deterministic. */
  cfg.enable_debug_overlay = 0u;

  zr_engine_t* e = NULL;
  zr_result_t rc = engine_create(&e, &cfg);
  if (rc != ZR_OK || !e) {
    return 2;
  }

  uint8_t drain[4096];
  (void)engine_poll_events(e, 0, drain, (int)sizeof(drain));

  zr_poll_thread_args_t args;
  memset(&args, 0, sizeof(args));
  args.e = e;
  const uint32_t tag = 0xC0FFEE01u;
  const uint8_t payload[] = {0xDEu, 0xADu, 0xBEu, 0xEFu};
  args.tag = tag;
  args.payload = payload;
  args.payload_len = (uint32_t)sizeof(payload);
  args.ready_event = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (!args.ready_event) {
    engine_destroy(e);
    return 2;
  }

  HANDLE th = CreateThread(NULL, 0u, zr_poll_thread, &args, 0u, NULL);
  if (!th) {
    CloseHandle(args.ready_event);
    engine_destroy(e);
    return 2;
  }

  if (WaitForSingleObject(args.ready_event, 1000u) != WAIT_OBJECT_0) {
    (void)WaitForSingleObject(th, 100u);
    CloseHandle(th);
    CloseHandle(args.ready_event);
    engine_destroy(e);
    return 2;
  }

  Sleep(50u);

  rc = engine_post_user_event(e, tag, payload, (int)sizeof(payload));
  if (rc != ZR_OK) {
    (void)WaitForSingleObject(th, 3000u);
    CloseHandle(th);
    CloseHandle(args.ready_event);
    engine_destroy(e);
    return 2;
  }

  DWORD join_rc = WaitForSingleObject(th, 3000u);
  CloseHandle(th);
  CloseHandle(args.ready_event);
  if (join_rc != WAIT_OBJECT_0) {
    engine_destroy(e);
    return 2;
  }

  if (!args.found_user) {
    engine_destroy(e);
    return 2;
  }

  engine_destroy(e);
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

  zr_result_t r = zr_win32_conpty_run_self_capture("--child", out, sizeof(out), &out_len, &exit_code, skip_reason,
                                                   sizeof(skip_reason));
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
