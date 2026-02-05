/*
  src/core/zr_engine.c — Public engine ABI implementation and orchestration.

  Why: Wires together platform I/O, input parsing, event batching, drawlist
  execution, framebuffer diff rendering, and single-flush output emission
  under the project's locked ownership and error contracts.
*/

#include "core/zr_engine.h"

#include "core/zr_diff.h"
#include "core/zr_debug_overlay.h"
#include "core/zr_drawlist.h"
#include "core/zr_event_pack.h"
#include "core/zr_event_queue.h"
#include "core/zr_input_parser.h"
#include "core/zr_metrics_internal.h"

#include "platform/zr_platform.h"

#include "util/zr_arena.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define ZR_ENGINE_INPUT_PENDING_CAP 64u

struct zr_engine_t {
  /* --- Platform (OS boundary) --- */
  plat_t* plat;

  plat_caps_t caps;
  plat_size_t size;

  /* --- Config (engine-owned copies) --- */
  zr_engine_config_t cfg_create;
  zr_engine_runtime_config_t cfg_runtime;

  /* --- Framebuffers (double buffered + staging for no-partial-effects) --- */
  zr_fb_t fb_prev;
  zr_fb_t fb_next;
  zr_fb_t fb_stage;

  zr_term_state_t term_state;

  /* --- Output buffer (single flush per present) --- */
  uint8_t* out_buf;
  size_t out_cap;

  /* --- Input/event pipeline --- */
  zr_event_queue_t evq;
  zr_event_t* ev_storage;
  uint32_t ev_cap;
  uint8_t* user_bytes;
  uint32_t user_bytes_cap;
  uint8_t input_pending[ZR_ENGINE_INPUT_PENDING_CAP];
  uint32_t input_pending_len;

  uint32_t last_present_ms;

  /* --- Arenas (reserved for future wiring; reset contract is enforced) --- */
  zr_arena_t arena_frame;
  zr_arena_t arena_persistent;

  /* --- Metrics snapshot (prefix-copied out) --- */
  zr_metrics_t metrics;
};

enum {
  ZR_ENGINE_EVENT_QUEUE_CAP = 1024u,
  ZR_ENGINE_USER_BYTES_CAP = 64u * 1024u,
  ZR_ENGINE_READ_CHUNK_CAP = 4096u,
  ZR_ENGINE_READ_LOOP_MAX = 64u,
};

static uint32_t zr_engine_now_ms_u32(void) {
  /* v1: time_ms is u32; truncation is deterministic and acceptable for telemetry. */
  return (uint32_t)plat_now_ms();
}

static uint64_t zr_engine_now_us_estimate(void) {
  /*
    Timing source is millisecond-resolution in v1. Convert to microseconds so
    metrics fields keep stable ABI units (best-effort precision).
  */
  return plat_now_ms() * 1000ull;
}

static uint32_t zr_engine_elapsed_us_saturated(uint64_t start_us, uint64_t end_us) {
  if (end_us <= start_us) {
    return 0u;
  }
  const uint64_t delta = end_us - start_us;
  if (delta > (uint64_t)UINT32_MAX) {
    return UINT32_MAX;
  }
  return (uint32_t)delta;
}

static void zr_engine_metrics_update_arena_high_water(zr_engine_t* e) {
  if (!e) {
    return;
  }

  const uint64_t frame_total = (uint64_t)e->arena_frame.total_bytes;
  if (frame_total > e->metrics.arena_frame_high_water_bytes) {
    e->metrics.arena_frame_high_water_bytes = frame_total;
  }

  const uint64_t persistent_total = (uint64_t)e->arena_persistent.total_bytes;
  if (persistent_total > e->metrics.arena_persistent_high_water_bytes) {
    e->metrics.arena_persistent_high_water_bytes = persistent_total;
  }
}

static size_t zr_engine_cells_bytes(const zr_fb_t* fb) {
  if (!fb || !fb->cells) {
    return 0u;
  }
  return (size_t)fb->cols * (size_t)fb->rows * sizeof(zr_cell_t);
}

static void zr_engine_fb_copy(const zr_fb_t* src, zr_fb_t* dst) {
  if (!src || !dst || !src->cells || !dst->cells) {
    return;
  }
  if (src->cols != dst->cols || src->rows != dst->rows) {
    return;
  }
  const size_t n = zr_engine_cells_bytes(src);
  if (n != 0u) {
    memcpy(dst->cells, src->cells, n);
  }
}

static void zr_engine_fb_swap(zr_fb_t* a, zr_fb_t* b) {
  if (!a || !b) {
    return;
  }
  zr_fb_t tmp = *a;
  *a = *b;
  *b = tmp;
}

/*
  Resize all engine framebuffers atomically.

  Why: Diff rendering assumes prev/next dimensions match. This helper allocates
  new backings for all buffers and commits only if all allocations succeed.
*/
static zr_result_t zr_engine_resize_framebuffers(zr_engine_t* e, uint32_t cols, uint32_t rows) {
  if (!e) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (cols == 0u || rows == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_fb_t prev = {0u, 0u, NULL};
  zr_fb_t next = {0u, 0u, NULL};
  zr_fb_t stage = {0u, 0u, NULL};

  zr_result_t rc = zr_fb_init(&prev, cols, rows);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_fb_init(&next, cols, rows);
  if (rc != ZR_OK) {
    zr_fb_release(&prev);
    return rc;
  }
  rc = zr_fb_init(&stage, cols, rows);
  if (rc != ZR_OK) {
    zr_fb_release(&prev);
    zr_fb_release(&next);
    return rc;
  }

  zr_fb_release(&e->fb_prev);
  zr_fb_release(&e->fb_next);
  zr_fb_release(&e->fb_stage);

  e->fb_prev = prev;
  e->fb_next = next;
  e->fb_stage = stage;

  /* A resize invalidates any cursor/style assumptions (best-effort). */
  memset(&e->term_state, 0, sizeof(e->term_state));

  return ZR_OK;
}

static zr_result_t zr_engine_try_handle_resize(zr_engine_t* e, uint32_t time_ms) {
  if (!e || !e->plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  plat_size_t sz;
  zr_result_t rc = plat_get_size(e->plat, &sz);
  if (rc != ZR_OK) {
    return rc;
  }
  if (sz.cols == e->size.cols && sz.rows == e->size.rows) {
    return ZR_OK;
  }

  rc = zr_engine_resize_framebuffers(e, sz.cols, sz.rows);
  if (rc != ZR_OK) {
    return rc;
  }

  e->size = sz;

  zr_event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ZR_EV_RESIZE;
  ev.time_ms = time_ms;
  ev.flags = 0u;
  ev.u.resize.cols = sz.cols;
  ev.u.resize.rows = sz.rows;
  ev.u.resize.reserved0 = 0u;
  ev.u.resize.reserved1 = 0u;
  (void)zr_event_queue_push(&e->evq, &ev);

  return ZR_OK;
}

/* Consume complete sequences from pending bytes without flushing trailing partials. */
static void zr_engine_input_pending_parse(zr_engine_t* e, uint32_t time_ms) {
  if (!e) {
    return;
  }

  for (;;) {
    const size_t pending_len = (size_t)e->input_pending_len;
    if (pending_len == 0u) {
      return;
    }

    const size_t consumed = zr_input_parse_bytes_prefix(&e->evq, e->input_pending, pending_len, time_ms);
    if (consumed == 0u || consumed > pending_len) {
      return;
    }

    const size_t remain = pending_len - consumed;
    if (remain != 0u) {
      memmove(e->input_pending, e->input_pending + consumed, remain);
    }
    e->input_pending_len = (uint32_t)remain;
  }
}

/* Append bytes to pending input and parse complete prefixes immediately. */
static void zr_engine_input_pending_append_bytes(zr_engine_t* e, const uint8_t* bytes, size_t len, uint32_t time_ms) {
  if (!e || (!bytes && len != 0u)) {
    return;
  }

  for (size_t i = 0u; i < len; i++) {
    if (e->input_pending_len >= (uint32_t)ZR_ENGINE_INPUT_PENDING_CAP) {
      /*
        Defensive bound: malformed/unsupported sequences must never grow the
        pending buffer unbounded. Flush pending deterministically and continue.
      */
      zr_input_parse_bytes(&e->evq, e->input_pending, (size_t)e->input_pending_len, time_ms);
      e->input_pending_len = 0u;
    }

    e->input_pending[e->input_pending_len++] = bytes[i];
    zr_engine_input_pending_parse(e, time_ms);
  }
}

/*
  Flush any trailing pending bytes as regular input.

  Why: A single ESC byte could represent either "Escape key" or a sequence
  prefix. On timeout/idleness we must commit pending bytes to avoid wedging.
*/
static void zr_engine_input_flush_pending(zr_engine_t* e, uint32_t time_ms) {
  if (!e || e->input_pending_len == 0u) {
    return;
  }
  zr_input_parse_bytes(&e->evq, e->input_pending, (size_t)e->input_pending_len, time_ms);
  e->input_pending_len = 0u;
}

static zr_result_t zr_engine_drain_platform_input(zr_engine_t* e, uint32_t time_ms) {
  if (!e || !e->plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  uint8_t buf[ZR_ENGINE_READ_CHUNK_CAP];

  for (uint32_t i = 0u; i < ZR_ENGINE_READ_LOOP_MAX; i++) {
    const int32_t n = plat_read_input(e->plat, buf, (int32_t)sizeof(buf));
    if (n < 0) {
      return (zr_result_t)n;
    }
    if (n == 0) {
      return ZR_OK;
    }
    zr_engine_input_pending_append_bytes(e, buf, (size_t)n, time_ms);
  }

  /* Defensive bound: platform must eventually report no more bytes to read. */
  return ZR_OK;
}

static bool zr_engine_pack_one_event(zr_evpack_writer_t* w, const zr_event_queue_t* q,
                                     const zr_event_t* ev) {
  if (!w || !q || !ev) {
    return false;
  }

  switch (ev->type) {
    case ZR_EV_KEY:
      return zr_evpack_append_record(w, ZR_EV_KEY, ev->time_ms, ev->flags, &ev->u.key, sizeof(ev->u.key));
    case ZR_EV_TEXT:
      return zr_evpack_append_record(w, ZR_EV_TEXT, ev->time_ms, ev->flags, &ev->u.text, sizeof(ev->u.text));
    case ZR_EV_MOUSE:
      return zr_evpack_append_record(w, ZR_EV_MOUSE, ev->time_ms, ev->flags, &ev->u.mouse, sizeof(ev->u.mouse));
    case ZR_EV_RESIZE:
      return zr_evpack_append_record(w, ZR_EV_RESIZE, ev->time_ms, ev->flags, &ev->u.resize, sizeof(ev->u.resize));
    case ZR_EV_TICK:
      return zr_evpack_append_record(w, ZR_EV_TICK, ev->time_ms, ev->flags, &ev->u.tick, sizeof(ev->u.tick));
    case ZR_EV_USER: {
      const uint8_t* payload = NULL;
      uint32_t payload_len = 0u;
      if (!zr_event_queue_user_payload_view(q, ev, &payload, &payload_len)) {
        return false;
      }
      return zr_evpack_append_record2(w, ZR_EV_USER, ev->time_ms, ev->flags, &ev->u.user.hdr,
                                      sizeof(ev->u.user.hdr), payload, (size_t)payload_len);
    }
    default:
      return false;
  }
}

/* Initialize the engine-owned runtime config from the create-time config. */
static void zr_engine_runtime_from_create_cfg(zr_engine_t* e, const zr_engine_config_t* cfg) {
  if (!e || !cfg) {
    return;
  }

  e->cfg_create = *cfg;

  e->cfg_runtime.limits = cfg->limits;
  e->cfg_runtime.plat = cfg->plat;
  e->cfg_runtime.tab_width = cfg->tab_width;
  e->cfg_runtime.width_policy = cfg->width_policy;
  e->cfg_runtime.target_fps = cfg->target_fps;
  e->cfg_runtime.enable_scroll_optimizations = cfg->enable_scroll_optimizations;
  e->cfg_runtime.enable_debug_overlay = cfg->enable_debug_overlay;
  e->cfg_runtime.enable_replay_recording = cfg->enable_replay_recording;
  e->cfg_runtime._pad0 = 0u;
}

/* Seed the metrics snapshot with negotiated ABI versions from create config. */
static void zr_engine_metrics_init(zr_engine_t* e, const zr_engine_config_t* cfg) {
  if (!e || !cfg) {
    return;
  }

  e->metrics = zr_metrics__default_snapshot();
  e->metrics.negotiated_engine_abi_major = cfg->requested_engine_abi_major;
  e->metrics.negotiated_engine_abi_minor = cfg->requested_engine_abi_minor;
  e->metrics.negotiated_engine_abi_patch = cfg->requested_engine_abi_patch;
  e->metrics.negotiated_drawlist_version = cfg->requested_drawlist_version;
  e->metrics.negotiated_event_batch_version = cfg->requested_event_batch_version;
  e->metrics.fps = cfg->target_fps;
}

static zr_result_t zr_engine_alloc_out_buf(zr_engine_t* e) {
  if (!e) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  e->out_cap = (size_t)e->cfg_runtime.limits.out_max_bytes_per_frame;
  e->out_buf = (uint8_t*)malloc(e->out_cap);
  if (!e->out_buf) {
    return ZR_ERR_OOM;
  }
  return ZR_OK;
}

static zr_result_t zr_engine_init_arenas(zr_engine_t* e) {
  if (!e) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_result_t rc = zr_arena_init(&e->arena_persistent, (size_t)e->cfg_runtime.limits.arena_initial_bytes,
                                 (size_t)e->cfg_runtime.limits.arena_max_total_bytes);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_arena_init(&e->arena_frame, (size_t)e->cfg_runtime.limits.arena_initial_bytes,
                     (size_t)e->cfg_runtime.limits.arena_max_total_bytes);
  if (rc != ZR_OK) {
    return rc;
  }

  zr_engine_metrics_update_arena_high_water(e);
  return ZR_OK;
}

static zr_result_t zr_engine_init_event_queue(zr_engine_t* e) {
  if (!e) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  e->ev_cap = ZR_ENGINE_EVENT_QUEUE_CAP;
  e->ev_storage = (zr_event_t*)calloc((size_t)e->ev_cap, sizeof(zr_event_t));
  if (!e->ev_storage) {
    return ZR_ERR_OOM;
  }
  e->user_bytes_cap = ZR_ENGINE_USER_BYTES_CAP;
  e->user_bytes = (uint8_t*)malloc((size_t)e->user_bytes_cap);
  if (!e->user_bytes) {
    return ZR_ERR_OOM;
  }
  return zr_event_queue_init(&e->evq, e->ev_storage, e->ev_cap, e->user_bytes, e->user_bytes_cap);
}

static zr_result_t zr_engine_init_platform(zr_engine_t* e) {
  if (!e) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_result_t rc = plat_create(&e->plat, &e->cfg_runtime.plat);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = plat_enter_raw(e->plat);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = plat_get_caps(e->plat, &e->caps);
  if (rc != ZR_OK) {
    return rc;
  }
  return plat_get_size(e->plat, &e->size);
}

/* Create an engine instance and enter raw mode on the configured platform backend. */
zr_result_t engine_create(zr_engine_t** out_engine, const zr_engine_config_t* cfg) {
  if (!out_engine || !cfg) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_engine = NULL;

  zr_result_t rc = zr_engine_config_validate(cfg);
  if (rc != ZR_OK) {
    return rc;
  }

  zr_engine_t* e = (zr_engine_t*)calloc(1u, sizeof(*e));
  if (!e) {
    return ZR_ERR_OOM;
  }

  zr_engine_runtime_from_create_cfg(e, cfg);
  zr_engine_metrics_init(e, cfg);

  rc = zr_engine_alloc_out_buf(e);
  if (rc != ZR_OK) {
    goto cleanup;
  }
  rc = zr_engine_init_arenas(e);
  if (rc != ZR_OK) {
    goto cleanup;
  }
  rc = zr_engine_init_event_queue(e);
  if (rc != ZR_OK) {
    goto cleanup;
  }
  rc = zr_engine_init_platform(e);
  if (rc != ZR_OK) {
    goto cleanup;
  }

  rc = zr_engine_resize_framebuffers(e, e->size.cols, e->size.rows);
  if (rc != ZR_OK) {
    goto cleanup;
  }

  *out_engine = e;
  return ZR_OK;

cleanup:
  engine_destroy(e);
  return rc;
}

/* Destroy an engine instance and restore best-effort platform state. */
void engine_destroy(zr_engine_t* e) {
  if (!e) {
    return;
  }

  if (e->plat) {
    (void)plat_leave_raw(e->plat);
    plat_destroy(e->plat);
    e->plat = NULL;
  }

  zr_fb_release(&e->fb_prev);
  zr_fb_release(&e->fb_next);
  zr_fb_release(&e->fb_stage);

  zr_arena_release(&e->arena_frame);
  zr_arena_release(&e->arena_persistent);

  free(e->out_buf);
  e->out_buf = NULL;
  e->out_cap = 0u;

  free(e->ev_storage);
  e->ev_storage = NULL;
  e->ev_cap = 0u;

  free(e->user_bytes);
  e->user_bytes = NULL;
  e->user_bytes_cap = 0u;

  free(e);
}

/*
  Validate and execute a drawlist against the staging framebuffer.

  Why: Enforces the "no partial effects" contract by only committing to fb_next
  after a successful execute.
*/
zr_result_t engine_submit_drawlist(zr_engine_t* e, const uint8_t* bytes, int bytes_len) {
  if (!e || !bytes) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (bytes_len < 0) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  const uint64_t t0_us = zr_engine_now_us_estimate();

  zr_dl_view_t v;
  zr_result_t rc = zr_dl_validate(bytes, (size_t)bytes_len, &e->cfg_runtime.limits, &v);
  if (rc != ZR_OK) {
    e->metrics.us_drawlist_last_frame = zr_engine_elapsed_us_saturated(t0_us, zr_engine_now_us_estimate());
    return rc;
  }

  zr_engine_fb_copy(&e->fb_next, &e->fb_stage);

  rc = zr_dl_execute(&v, &e->fb_stage, &e->cfg_runtime.limits, (zr_width_policy_t)e->cfg_runtime.width_policy);
  if (rc != ZR_OK) {
    e->metrics.us_drawlist_last_frame = zr_engine_elapsed_us_saturated(t0_us, zr_engine_now_us_estimate());
    return rc;
  }

  zr_engine_fb_swap(&e->fb_next, &e->fb_stage);
  e->metrics.us_drawlist_last_frame = zr_engine_elapsed_us_saturated(t0_us, zr_engine_now_us_estimate());
  zr_engine_metrics_update_arena_high_water(e);
  return ZR_OK;
}

static zr_result_t zr_engine_present_render(zr_engine_t* e,
                                            size_t* out_len,
                                            zr_term_state_t* final_ts,
                                            zr_diff_stats_t* stats,
                                            bool* out_used_stage) {
  if (!e || !out_len || !final_ts || !stats || !out_used_stage) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  const zr_fb_t* diff_next = &e->fb_next;
  bool used_stage = false;

  if (e->cfg_runtime.enable_debug_overlay != 0u) {
    zr_engine_fb_copy(&e->fb_next, &e->fb_stage);
    zr_result_t orc = zr_debug_overlay_render(&e->fb_stage, &e->metrics);
    if (orc != ZR_OK) {
      return orc;
    }
    diff_next = &e->fb_stage;
    used_stage = true;
  }

  const uint64_t t0_us = zr_engine_now_us_estimate();
  zr_result_t rc =
      zr_diff_render_with_opts(&e->fb_prev, diff_next, &e->caps, &e->term_state,
                               e->cfg_runtime.enable_scroll_optimizations, e->out_buf, e->out_cap, out_len, final_ts,
                               stats);
  e->metrics.us_diff_last_frame = zr_engine_elapsed_us_saturated(t0_us, zr_engine_now_us_estimate());
  if (rc != ZR_OK) {
    return rc;
  }
  if (*out_len > (size_t)INT32_MAX) {
    return ZR_ERR_LIMIT;
  }
  *out_used_stage = used_stage;
  return ZR_OK;
}

static zr_result_t zr_engine_present_write(zr_engine_t* e, size_t out_len) {
  if (!e || !e->plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return plat_write_output(e->plat, e->out_buf, (int32_t)out_len);
}

static void zr_engine_present_commit(zr_engine_t* e,
                                     size_t out_len,
                                     const zr_term_state_t* final_ts,
                                     const zr_diff_stats_t* stats,
                                     bool used_stage) {
  if (!e || !final_ts || !stats) {
    return;
  }

  if (used_stage) {
    zr_engine_fb_copy(&e->fb_stage, &e->fb_next);
  }
  zr_engine_fb_swap(&e->fb_prev, &e->fb_next);
  e->term_state = *final_ts;

  e->metrics.frame_index++;
  e->metrics.bytes_emitted_total += (uint64_t)out_len;
  e->metrics.bytes_emitted_last_frame = (uint32_t)out_len;
  e->metrics.dirty_lines_last_frame = stats->dirty_lines;
  e->metrics.dirty_cols_last_frame = stats->dirty_cells;

  const uint32_t now_ms = zr_engine_now_ms_u32();
  if (e->last_present_ms != 0u) {
    const uint32_t elapsed_ms = now_ms - e->last_present_ms;
    if (elapsed_ms != 0u) {
      uint32_t fps = 1000u / elapsed_ms;
      if (fps == 0u) {
        fps = 1u;
      }
      e->metrics.fps = fps;
    }
  } else if (e->cfg_runtime.target_fps != 0u) {
    e->metrics.fps = e->cfg_runtime.target_fps;
  }
  e->last_present_ms = now_ms;

  zr_engine_metrics_update_arena_high_water(e);
}

/*
  Render and flush the framebuffer diff to the platform backend.

  Why: Enforces the single-flush-per-present contract by calling plat_write_output()
  exactly once on success and never writing on failure.
*/
zr_result_t engine_present(zr_engine_t* e) {
  if (!e || !e->plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  /* Enforced contract: the per-frame arena is reset exactly once per present. */
  zr_arena_reset(&e->arena_frame);

  size_t out_len = 0u;
  zr_term_state_t final_ts;
  zr_diff_stats_t stats;
  bool used_stage = false;

  zr_result_t rc = zr_engine_present_render(e, &out_len, &final_ts, &stats, &used_stage);
  if (rc != ZR_OK) {
    return rc;
  }
  const uint64_t t_write0_us = zr_engine_now_us_estimate();
  rc = zr_engine_present_write(e, out_len);
  e->metrics.us_write_last_frame = zr_engine_elapsed_us_saturated(t_write0_us, zr_engine_now_us_estimate());
  if (rc != ZR_OK) {
    return rc;
  }

  zr_engine_present_commit(e, out_len, &final_ts, &stats, used_stage);
  return ZR_OK;
}

static int zr_engine_poll_wait_and_fill(zr_engine_t* e, int timeout_ms, uint32_t time_ms) {
  if (!e || !e->plat) {
    return (int)ZR_ERR_INVALID_ARGUMENT;
  }

  if (zr_event_queue_count(&e->evq) != 0u) {
    return 1;
  }

  const int32_t w = plat_wait(e->plat, (int32_t)timeout_ms);
  if (w < 0) {
    return (int)w;
  }
  if (w == 0) {
    zr_result_t rc = zr_engine_try_handle_resize(e, time_ms);
    if (rc != ZR_OK) {
      return (int)rc;
    }
    zr_engine_input_flush_pending(e, time_ms);
    return zr_event_queue_count(&e->evq) != 0u ? 1 : 0;
  }

  zr_result_t rc = zr_engine_try_handle_resize(e, time_ms);
  if (rc != ZR_OK) {
    return (int)rc;
  }
  rc = zr_engine_drain_platform_input(e, time_ms);
  if (rc != ZR_OK) {
    return (int)rc;
  }
  return 1;
}

static int zr_engine_poll_pack(zr_engine_t* e, uint8_t* out_buf, int out_cap) {
  if (!e) {
    return (int)ZR_ERR_INVALID_ARGUMENT;
  }

  zr_evpack_writer_t w;
  zr_result_t rc = zr_evpack_begin(&w, out_buf, (size_t)out_cap);
  if (rc != ZR_OK) {
    return (int)rc;
  }

  zr_event_t ev;
  while (zr_event_queue_peek(&e->evq, &ev)) {
    if (!zr_engine_pack_one_event(&w, &e->evq, &ev)) {
      break;
    }
    (void)zr_event_queue_pop(&e->evq, &ev);
  }

  const size_t bytes_written = zr_evpack_finish(&w);
  e->metrics.events_out_last_poll = w.event_count;
  e->metrics.events_dropped_total = e->evq.dropped_total;

  if (bytes_written > (size_t)INT_MAX) {
    return (int)ZR_ERR_LIMIT;
  }
  return (int)bytes_written;
}

/*
  Poll input/events and pack a batch for the caller.

  Why: Waits only when the internal queue is empty, then emits a packed batch
  with success-mode truncation and no writes on errors.
*/
int engine_poll_events(zr_engine_t* e, int timeout_ms, uint8_t* out_buf, int out_cap) {
  if (!e || !e->plat) {
    return (int)ZR_ERR_INVALID_ARGUMENT;
  }
  if (timeout_ms < 0) {
    return (int)ZR_ERR_INVALID_ARGUMENT;
  }
  if (out_cap < 0) {
    return (int)ZR_ERR_INVALID_ARGUMENT;
  }
  if (out_cap > 0 && !out_buf) {
    return (int)ZR_ERR_INVALID_ARGUMENT;
  }

  const uint64_t t0_us = zr_engine_now_us_estimate();
  const uint32_t time_ms = zr_engine_now_ms_u32();

  const int ready = zr_engine_poll_wait_and_fill(e, timeout_ms, time_ms);
  e->metrics.us_input_last_frame = zr_engine_elapsed_us_saturated(t0_us, zr_engine_now_us_estimate());
  if (ready <= 0) {
    return ready;
  }

  if (zr_event_queue_count(&e->evq) == 0u) {
    return 0;
  }
  return zr_engine_poll_pack(e, out_buf, out_cap);
}

/* Queue a user event and best-effort wake the platform wait. */
zr_result_t engine_post_user_event(zr_engine_t* e, uint32_t tag, const uint8_t* payload, int payload_len) {
  if (!e || !e->plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (payload_len < 0) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (payload_len != 0 && !payload) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  const uint32_t time_ms = zr_engine_now_ms_u32();
  zr_result_t rc = zr_event_queue_post_user(&e->evq, time_ms, tag, payload, (uint32_t)payload_len);
  if (rc != ZR_OK) {
    return rc;
  }

  /* Best-effort wake (thread-safe), but do not introduce partial failures. */
  (void)plat_wake(e->plat);
  return ZR_OK;
}

/* Copy out a stable metrics snapshot for telemetry/debug. */
zr_result_t engine_get_metrics(zr_engine_t* e, zr_metrics_t* out_metrics) {
  if (!e || !out_metrics) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_metrics__copy_out(out_metrics, &e->metrics);
}

static zr_result_t zr_engine_set_config_prepare_out_buf(zr_engine_t* e,
                                                        const zr_engine_runtime_config_t* cfg,
                                                        uint8_t** out_buf_new,
                                                        size_t* out_cap_new,
                                                        bool* want_out_buf) {
  if (!e || !cfg || !out_buf_new || !out_cap_new || !want_out_buf) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  *out_buf_new = NULL;
  *out_cap_new = e->out_cap;
  *want_out_buf = (cfg->limits.out_max_bytes_per_frame != e->cfg_runtime.limits.out_max_bytes_per_frame);

  if (!*want_out_buf) {
    return ZR_OK;
  }

  *out_cap_new = (size_t)cfg->limits.out_max_bytes_per_frame;
  *out_buf_new = (uint8_t*)malloc(*out_cap_new);
  if (!*out_buf_new) {
    return ZR_ERR_OOM;
  }
  return ZR_OK;
}

static zr_result_t zr_engine_set_config_prepare_arenas(zr_engine_t* e,
                                                       const zr_engine_runtime_config_t* cfg,
                                                       zr_arena_t* arena_frame_new,
                                                       zr_arena_t* arena_persistent_new,
                                                       bool* want_arena_reinit) {
  if (!e || !cfg || !arena_frame_new || !arena_persistent_new || !want_arena_reinit) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  memset(arena_frame_new, 0, sizeof(*arena_frame_new));
  memset(arena_persistent_new, 0, sizeof(*arena_persistent_new));

  *want_arena_reinit = (cfg->limits.arena_initial_bytes != e->cfg_runtime.limits.arena_initial_bytes) ||
                       (cfg->limits.arena_max_total_bytes != e->cfg_runtime.limits.arena_max_total_bytes);
  if (!*want_arena_reinit) {
    return ZR_OK;
  }

  zr_result_t rc = zr_arena_init(arena_frame_new, (size_t)cfg->limits.arena_initial_bytes,
                                 (size_t)cfg->limits.arena_max_total_bytes);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_arena_init(arena_persistent_new, (size_t)cfg->limits.arena_initial_bytes,
                     (size_t)cfg->limits.arena_max_total_bytes);
  if (rc != ZR_OK) {
    zr_arena_release(arena_frame_new);
    memset(arena_frame_new, 0, sizeof(*arena_frame_new));
    return rc;
  }
  return ZR_OK;
}

static void zr_engine_set_config_commit(zr_engine_t* e,
                                        const zr_engine_runtime_config_t* cfg,
                                        bool want_out_buf,
                                        uint8_t** out_buf_new,
                                        size_t out_cap_new,
                                        bool want_arena_reinit,
                                        zr_arena_t* arena_frame_new,
                                        zr_arena_t* arena_persistent_new) {
  if (!e || !cfg || !out_buf_new || !arena_frame_new || !arena_persistent_new) {
    return;
  }

  if (want_out_buf) {
    free(e->out_buf);
    e->out_buf = *out_buf_new;
    e->out_cap = out_cap_new;
    *out_buf_new = NULL;
  }

  if (want_arena_reinit) {
    zr_arena_release(&e->arena_frame);
    zr_arena_release(&e->arena_persistent);
    e->arena_frame = *arena_frame_new;
    e->arena_persistent = *arena_persistent_new;
    memset(arena_frame_new, 0, sizeof(*arena_frame_new));
    memset(arena_persistent_new, 0, sizeof(*arena_persistent_new));
  }

  e->cfg_runtime = *cfg;
  zr_engine_metrics_update_arena_high_water(e);
}

/*
  Update engine-owned runtime config.

  Why: Applies only after all required allocations succeed ("no partial effects").
*/
zr_result_t engine_set_config(zr_engine_t* e, const zr_engine_runtime_config_t* cfg) {
  if (!e || !cfg) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  zr_result_t rc = zr_engine_runtime_config_validate(cfg);
  if (rc != ZR_OK) {
    return rc;
  }

  if (memcmp(&cfg->plat, &e->cfg_runtime.plat, sizeof(cfg->plat)) != 0) {
    return ZR_ERR_UNSUPPORTED;
  }

  uint8_t* out_buf_new = NULL;
  size_t out_cap_new = e->out_cap;
  zr_arena_t arena_frame_new = {0};
  zr_arena_t arena_persistent_new = {0};

  bool want_out_buf = false;
  bool want_arena_reinit = false;

  rc = zr_engine_set_config_prepare_out_buf(e, cfg, &out_buf_new, &out_cap_new, &want_out_buf);
  if (rc != ZR_OK) {
    goto cleanup;
  }
  rc = zr_engine_set_config_prepare_arenas(e, cfg, &arena_frame_new, &arena_persistent_new, &want_arena_reinit);
  if (rc != ZR_OK) {
    goto cleanup;
  }

  /* Commit (no partial effects): allocations succeeded; now swap in new resources. */
  zr_engine_set_config_commit(e, cfg, want_out_buf, &out_buf_new, out_cap_new, want_arena_reinit, &arena_frame_new,
                              &arena_persistent_new);
  return ZR_OK;

cleanup:
  free(out_buf_new);
  zr_arena_release(&arena_frame_new);
  zr_arena_release(&arena_persistent_new);
  return rc;
}
