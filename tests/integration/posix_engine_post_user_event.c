/*
  tests/integration/posix_engine_post_user_event.c — Engine-level cross-thread user-event wake test.

  Why: Verifies that engine_post_user_event() can be called from a non-engine
  thread, wakes a blocked engine_poll_events(), and preserves payload bytes in
  the packed event batch.
*/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "zr/zr_engine.h"
#include "zr/zr_event.h"
#include "zr/zr_version.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

enum {
  ZR_TEST_PTY_COLS = 120,
  ZR_TEST_PTY_ROWS = 40,
};

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

  *out_master_fd = master_fd;
  *out_slave_fd = slave_fd;
  return 0;
}

/* Set deterministic PTY geometry for engine_create() size probing. */
static int zr_set_pty_size(int fd, uint16_t cols, uint16_t rows) {
  if (fd < 0 || cols == 0u || rows == 0u) {
    return -1;
  }

  struct winsize ws;
  memset(&ws, 0, sizeof(ws));
  ws.ws_col = cols;
  ws.ws_row = rows;
  return (ioctl(fd, TIOCSWINSZ, &ws) == 0) ? 0 : -1;
}

typedef struct zr_poll_thread_args_t {
  zr_engine_t* e;
  uint32_t tag;
  const uint8_t* payload;
  uint32_t payload_len;
  bool found_user;
  int n_last;
  uint8_t out[4096];
} zr_poll_thread_args_t;

static bool zr_batch_has_user_event(const uint8_t* buf, size_t len, uint32_t tag, const uint8_t* payload,
                                    uint32_t payload_len);

static void* zr_poll_thread(void* user) {
  enum {
    ZR_POLL_SLICE_MS = 250,
    ZR_POLL_MAX_ITERS = 20,
  };

  zr_poll_thread_args_t* args = (zr_poll_thread_args_t*)user;
  args->found_user = false;
  args->n_last = 0;

  for (int i = 0; i < ZR_POLL_MAX_ITERS; i++) {
    const int n = engine_poll_events(args->e, ZR_POLL_SLICE_MS, args->out, (int)sizeof(args->out));
    args->n_last = n;
    if (n < 0) {
      return NULL;
    }
    if (n == 0) {
      continue;
    }
    if (zr_batch_has_user_event(args->out, (size_t)n, args->tag, args->payload, args->payload_len)) {
      args->found_user = true;
      return NULL;
    }
  }
  return NULL;
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

int main(void) {
  int master_fd = -1;
  int slave_fd = -1;
  if (zr_make_pty_pair(&master_fd, &slave_fd) != 0) {
    return zr_test_skip("PTY APIs not available (posix_openpt/grantpt/unlockpt/ptsname/open)");
  }
  if (zr_set_pty_size(slave_fd, ZR_TEST_PTY_COLS, ZR_TEST_PTY_ROWS) != 0) {
    fprintf(stderr, "TIOCSWINSZ failed: errno=%d\n", errno);
    (void)close(master_fd);
    (void)close(slave_fd);
    return 2;
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

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.requested_engine_abi_major = ZR_ENGINE_ABI_MAJOR;
  cfg.requested_engine_abi_minor = ZR_ENGINE_ABI_MINOR;
  cfg.requested_engine_abi_patch = ZR_ENGINE_ABI_PATCH;
  cfg.requested_drawlist_version = ZR_DRAWLIST_VERSION_V1;
  cfg.requested_event_batch_version = ZR_EVENT_BATCH_VERSION_V1;
  cfg.target_fps = 0u; /* Disable periodic ZR_EV_TICK to keep wake source deterministic. */
  cfg.enable_debug_overlay = 0u;

  zr_engine_t* e = NULL;
  zr_result_t rc = engine_create(&e, &cfg);
  if (rc != ZR_OK || !e) {
    fprintf(stderr, "engine_create() failed: rc=%d\n", (int)rc);
    (void)close(master_fd);
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

  pthread_t th;
  if (pthread_create(&th, NULL, zr_poll_thread, &args) != 0) {
    fprintf(stderr, "pthread_create() failed\n");
    engine_destroy(e);
    (void)close(master_fd);
    return 2;
  }

  zr_sleep_ms(50);

  rc = engine_post_user_event(e, tag, payload, (int)sizeof(payload));
  if (rc != ZR_OK) {
    fprintf(stderr, "engine_post_user_event() failed: rc=%d\n", (int)rc);
    (void)pthread_join(th, NULL);
    engine_destroy(e);
    (void)close(master_fd);
    return 2;
  }

  (void)pthread_join(th, NULL);

  if (!args.found_user) {
    fprintf(stderr, "packed batch missing posted user event (last_poll=%d)\n", args.n_last);
    engine_destroy(e);
    (void)close(master_fd);
    return 2;
  }

  engine_destroy(e);
  (void)close(master_fd);
  return 0;
}
