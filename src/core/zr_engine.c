/*
  src/core/zr_engine.c — Public engine ABI implementation and orchestration.

  Why: Wires together platform I/O, input parsing, event batching, drawlist
  execution, framebuffer diff rendering, and single-flush output emission
  under the project's locked ownership and error contracts.
*/

#include "core/zr_engine.h"

#include "core/zr_cursor.h"
#include "core/zr_damage.h"
#include "core/zr_debug_trace.h"
#include "core/zr_diff.h"
#include "core/zr_drawlist.h"
#include "core/zr_event_pack.h"
#include "core/zr_event_queue.h"
#include "core/zr_input_parser.h"
#include "core/zr_metrics_internal.h"

#include "platform/zr_platform.h"

#include "util/zr_arena.h"
#include "util/zr_checked.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct zr_engine_t {
  /* --- Platform (OS boundary) --- */
  plat_t* plat;

  plat_caps_t caps;
  plat_size_t size;

  /* --- Config (engine-owned copies) --- */
  zr_engine_config_t cfg_create;
  zr_engine_runtime_config_t cfg_runtime;

  /* --- Tick scheduling (ZR_EV_TICK emission) --- */
  uint32_t last_tick_ms;

  /* --- Framebuffers (double buffered + staging for no-partial-effects) --- */
  zr_fb_t fb_prev;
  zr_fb_t fb_next;
  zr_fb_t fb_stage;

  zr_term_state_t term_state;
  zr_cursor_state_t cursor_desired;

  /* --- Output buffer (single flush per present) --- */
  uint8_t* out_buf;
  size_t out_cap;

  /* --- Damage scratch (rect list is internal; only metrics are exported) --- */
  zr_damage_rect_t* damage_rects;
  uint32_t          damage_rect_cap;

  /* --- Input/event pipeline --- */
  zr_event_queue_t evq;
  zr_event_t* ev_storage;
  uint32_t ev_cap;
  uint8_t* user_bytes;
  uint32_t user_bytes_cap;

  /* --- Arenas (reserved for future wiring; reset contract is enforced) --- */
  zr_arena_t arena_frame;
  zr_arena_t arena_persistent;

  /* --- Metrics snapshot (prefix-copied out) --- */
  zr_metrics_t metrics;

  /* --- Debug trace (optional, engine-owned) --- */
  zr_debug_trace_t* debug_trace;
  uint8_t* debug_ring_buf;
  uint32_t* debug_record_offsets;
  uint32_t* debug_record_sizes;
};

enum {
  ZR_ENGINE_EVENT_QUEUE_CAP = 1024u,
  ZR_ENGINE_USER_BYTES_CAP = 64u * 1024u,
  ZR_ENGINE_READ_CHUNK_CAP = 4096u,
  ZR_ENGINE_READ_LOOP_MAX = 64u,
  ZR_ENGINE_DEFAULT_TICK_INTERVAL_MS = 16u,
};

/* Forward declaration for cleanup helper. */
static void zr_engine_debug_free(zr_engine_t* e);

static const uint8_t ZR_SYNC_BEGIN[] = "\x1b[?2026h";
static const uint8_t ZR_SYNC_END[] = "\x1b[?2026l";

static uint32_t zr_engine_now_ms_u32(void) {
  /* v1: time_ms is u32; truncation is deterministic and acceptable for telemetry. */
  return (uint32_t)plat_now_ms();
}

static uint32_t zr_engine_tick_interval_ms(const zr_engine_runtime_config_t* cfg) {
  if (!cfg || cfg->target_fps == 0u) {
    return ZR_ENGINE_DEFAULT_TICK_INTERVAL_MS;
  }
  uint32_t ms_u32 = 1000u / cfg->target_fps;
  if (ms_u32 == 0u) {
    ms_u32 = 1u;
  }
  return ms_u32;
}

static uint32_t zr_engine_tick_until_due_ms(const zr_engine_t* e, uint32_t now_ms) {
  if (!e) {
    return 0u;
  }
  const uint32_t interval_ms = zr_engine_tick_interval_ms(&e->cfg_runtime);
  const uint32_t elapsed_ms = now_ms - e->last_tick_ms;
  if (elapsed_ms >= interval_ms) {
    return 0u;
  }
  return interval_ms - elapsed_ms;
}

/*
  Best-effort periodic tick insertion.

  Why: Wrappers rely on ZR_EV_TICK for animation/perf overlays even when there
  is no input. Ticks must not evict existing input events; if the queue is full,
  the tick is dropped silently and poll continues.
*/
static void zr_engine_maybe_enqueue_tick(zr_engine_t* e, uint32_t now_ms) {
  if (!e) {
    return;
  }

  const uint32_t interval_ms = zr_engine_tick_interval_ms(&e->cfg_runtime);
  const uint32_t elapsed_ms = now_ms - e->last_tick_ms;
  if (elapsed_ms < interval_ms) {
    return;
  }

  uint32_t dt_ms = elapsed_ms;
  if (dt_ms == 0u) {
    dt_ms = 1u;
  }

  zr_event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ZR_EV_TICK;
  ev.time_ms = now_ms;
  ev.flags = 0u;
  ev.u.tick.dt_ms = dt_ms;
  ev.u.tick.reserved0 = 0u;
  ev.u.tick.reserved1 = 0u;
  ev.u.tick.reserved2 = 0u;

  (void)zr_event_queue_try_push_no_drop(&e->evq, &ev);

  /* Advance regardless of queue space to avoid repeated tick attempts. */
  e->last_tick_ms = now_ms;
}

static zr_cursor_state_t zr_engine_cursor_default(void) {
  zr_cursor_state_t s;
  s.x = -1;
  s.y = -1;
  s.shape = ZR_CURSOR_SHAPE_BLOCK;
  s.visible = 0u;
  s.blink = 0u;
  s.reserved0 = 0u;
  return s;
}

static size_t zr_engine_cells_bytes(const zr_fb_t* fb) {
  if (!fb || !fb->cells) {
    return 0u;
  }
  return (size_t)fb->cols * (size_t)fb->rows * sizeof(zr_cell_t);
}

static int32_t zr_engine_output_wait_timeout_ms(const zr_engine_runtime_config_t* cfg) {
  if (!cfg) {
    return 0;
  }
  if (cfg->target_fps == 0u) {
    return 0;
  }
  uint32_t ms_u32 = 1000u / cfg->target_fps;
  if (ms_u32 == 0u) {
    ms_u32 = 1u;
  }
  if (ms_u32 > (uint32_t)INT32_MAX) {
    ms_u32 = (uint32_t)INT32_MAX;
  }
  return (int32_t)ms_u32;
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
    zr_input_parse_bytes(&e->evq, buf, (size_t)n, time_ms);
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
  e->cfg_runtime.wait_for_output_drain = cfg->wait_for_output_drain;
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

static zr_result_t zr_engine_alloc_damage_rects(zr_engine_t* e) {
  if (!e) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  e->damage_rects = NULL;
  e->damage_rect_cap = 0u;

  const uint32_t cap = e->cfg_runtime.limits.diff_max_damage_rects;
  if (cap == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  size_t bytes = 0u;
  if (!zr_checked_mul_size((size_t)cap, sizeof(zr_damage_rect_t), &bytes)) {
    return ZR_ERR_LIMIT;
  }

  e->damage_rects = (zr_damage_rect_t*)calloc(cap, sizeof(zr_damage_rect_t));
  if (!e->damage_rects) {
    return ZR_ERR_OOM;
  }
  e->damage_rect_cap = cap;
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
  return zr_arena_init(&e->arena_frame, (size_t)e->cfg_runtime.limits.arena_initial_bytes,
                       (size_t)e->cfg_runtime.limits.arena_max_total_bytes);
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

  e->cursor_desired = zr_engine_cursor_default();
  e->last_tick_ms = zr_engine_now_ms_u32();

  zr_engine_runtime_from_create_cfg(e, cfg);
  zr_engine_metrics_init(e, cfg);

  rc = zr_engine_alloc_out_buf(e);
  if (rc != ZR_OK) {
    goto cleanup;
  }
  rc = zr_engine_alloc_damage_rects(e);
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

  /* Re-seed after backend init in case creation time was far from polling time. */
  e->last_tick_ms = zr_engine_now_ms_u32();

  /*
    Emit an initial resize event.

    Why: Wrappers frequently size their viewport from ZR_EV_RESIZE. Some terminal
    environments can report stale dimensions to wrappers at startup, and the
    engine itself will not emit a resize event until the size changes. Enqueue
    the initial size so callers can render the full framebuffer immediately.
  */
  {
    zr_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = ZR_EV_RESIZE;
    ev.time_ms = e->last_tick_ms;
    ev.flags = 0u;
    ev.u.resize.cols = e->size.cols;
    ev.u.resize.rows = e->size.rows;
    ev.u.resize.reserved0 = 0u;
    ev.u.resize.reserved1 = 0u;
    (void)zr_event_queue_push(&e->evq, &ev);
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

  free(e->damage_rects);
  e->damage_rects = NULL;
  e->damage_rect_cap = 0u;

  free(e->ev_storage);
  e->ev_storage = NULL;
  e->ev_cap = 0u;

  free(e->user_bytes);
  e->user_bytes = NULL;
  e->user_bytes_cap = 0u;

  zr_engine_debug_free(e);

  free(e);
}

/* Get current time in microseconds for debug tracing. */
static uint64_t zr_engine_now_us(void) {
  return (uint64_t)plat_now_ms() * 1000u;
}

/*
  Compute the debug-trace frame id for the next present.

  Why: metrics.frame_index increments at the end of engine_present(). For trace
  correlation, treat the next present as (frame_index + 1).
*/
static uint64_t zr_engine_trace_frame_id(const zr_engine_t* e) {
  if (!e) {
    return 0u;
  }
  if (e->metrics.frame_index == UINT64_MAX) {
    return UINT64_MAX;
  }
  return e->metrics.frame_index + 1u;
}

/*
  Record a drawlist debug trace if tracing is enabled.
*/
static void zr_engine_trace_drawlist(zr_engine_t* e,
                                     uint32_t code,
                                     const uint8_t* bytes,
                                     uint32_t bytes_len,
                                     uint32_t cmd_count,
                                     uint32_t version,
                                     zr_result_t validation_result,
                                     zr_result_t execution_result) {
  if (!e || !e->debug_trace) {
    return;
  }
  if (!zr_debug_trace_enabled(e->debug_trace, ZR_DEBUG_CAT_DRAWLIST, ZR_DEBUG_SEV_INFO)) {
    return;
  }

  const uint64_t frame_id = zr_engine_trace_frame_id(e);
  zr_debug_trace_set_frame(e->debug_trace, frame_id);

  if (e->debug_trace->config.capture_drawlist_bytes != 0u && bytes && bytes_len != 0u) {
    uint32_t n = bytes_len;
    if (n > (uint32_t)ZR_DEBUG_MAX_PAYLOAD_SIZE) {
      n = (uint32_t)ZR_DEBUG_MAX_PAYLOAD_SIZE;
    }
    (void)zr_debug_trace_record(e->debug_trace, ZR_DEBUG_CAT_DRAWLIST, ZR_DEBUG_SEV_TRACE,
                                ZR_DEBUG_CODE_DRAWLIST_CMD, zr_engine_now_us(), bytes, n);
  }

  zr_debug_drawlist_record_t rec;
  memset(&rec, 0, sizeof(rec));
  rec.frame_id = frame_id;
  rec.total_bytes = bytes_len;
  rec.cmd_count = cmd_count;
  rec.version = version;
  rec.validation_result = (uint32_t)validation_result;
  rec.execution_result = (uint32_t)execution_result;

  (void)zr_debug_trace_drawlist(e->debug_trace, code, zr_engine_now_us(), &rec);
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

  zr_dl_view_t v;
  zr_result_t rc = zr_dl_validate(bytes, (size_t)bytes_len, &e->cfg_runtime.limits, &v);
  if (rc != ZR_OK) {
    zr_engine_trace_drawlist(e, ZR_DEBUG_CODE_DRAWLIST_VALIDATE, bytes,
                             (uint32_t)bytes_len, 0u, 0u, rc, ZR_OK);
    return rc;
  }

  zr_engine_fb_copy(&e->fb_next, &e->fb_stage);

  zr_cursor_state_t cursor_stage = e->cursor_desired;
  rc = zr_dl_execute(&v, &e->fb_stage, &e->cfg_runtime.limits, &cursor_stage);
  if (rc != ZR_OK) {
    zr_engine_trace_drawlist(e, ZR_DEBUG_CODE_DRAWLIST_EXECUTE, bytes,
                             (uint32_t)bytes_len, v.hdr.cmd_count, v.hdr.version, ZR_OK, rc);
    return rc;
  }

  zr_engine_fb_swap(&e->fb_next, &e->fb_stage);
  e->cursor_desired = cursor_stage;

  zr_engine_trace_drawlist(e, ZR_DEBUG_CODE_DRAWLIST_EXECUTE, bytes,
                           (uint32_t)bytes_len, v.hdr.cmd_count, v.hdr.version, ZR_OK, ZR_OK);

  return ZR_OK;
}

static zr_result_t zr_engine_present_render(zr_engine_t* e,
                                            size_t* out_len,
                                            zr_term_state_t* final_ts,
                                            zr_diff_stats_t* stats) {
  if (!e || !out_len || !final_ts || !stats) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_result_t rc = zr_diff_render(&e->fb_prev, &e->fb_next, &e->caps, &e->term_state, &e->cursor_desired,
                                  &e->cfg_runtime.limits, e->damage_rects, e->damage_rect_cap,
                                  e->cfg_runtime.enable_scroll_optimizations, e->out_buf, e->out_cap, out_len, final_ts,
                                  stats);
  if (rc != ZR_OK) {
    return rc;
  }

  if (e->caps.supports_sync_update != 0u) {
    const size_t prefix_len = sizeof(ZR_SYNC_BEGIN) - 1u;
    const size_t suffix_len = sizeof(ZR_SYNC_END) - 1u;
    const size_t overhead = prefix_len + suffix_len;
    if ((*out_len + overhead) <= e->out_cap) {
      memmove(e->out_buf + prefix_len, e->out_buf, *out_len);
      memcpy(e->out_buf, ZR_SYNC_BEGIN, prefix_len);
      memcpy(e->out_buf + prefix_len + *out_len, ZR_SYNC_END, suffix_len);
      *out_len += overhead;
      stats->bytes_emitted = *out_len;
    }
  }

  if (*out_len > (size_t)INT32_MAX) {
    *out_len = 0u;
    memset(final_ts, 0, sizeof(*final_ts));
    memset(stats, 0, sizeof(*stats));
    return ZR_ERR_LIMIT;
  }
  return ZR_OK;
}

static zr_result_t zr_engine_present_write(zr_engine_t* e, size_t out_len) {
  if (!e || !e->plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return plat_write_output(e->plat, e->out_buf, (int32_t)out_len);
}

/*
  Record a frame debug trace after present commits.
*/
static void zr_engine_trace_frame(zr_engine_t* e,
                                  uint64_t frame_id,
                                  size_t out_len,
                                  const zr_diff_stats_t* stats) {
  if (!e || !e->debug_trace || !stats) {
    return;
  }
  if (!zr_debug_trace_enabled(e->debug_trace, ZR_DEBUG_CAT_FRAME, ZR_DEBUG_SEV_INFO)) {
    return;
  }

  zr_debug_trace_set_frame(e->debug_trace, frame_id);

  zr_debug_frame_record_t rec;
  memset(&rec, 0, sizeof(rec));
  rec.frame_id = frame_id;
  rec.cols = e->fb_next.cols;
  rec.rows = e->fb_next.rows;
  rec.diff_bytes_emitted = (uint32_t)out_len;
  rec.dirty_lines = stats->dirty_lines;
  rec.dirty_cells = stats->dirty_cells;
  rec.damage_rects = stats->damage_rects;

  (void)zr_debug_trace_frame(e->debug_trace, ZR_DEBUG_CODE_FRAME_PRESENT,
                             zr_engine_now_us(), &rec);
}

static void zr_engine_present_commit(zr_engine_t* e,
                                     size_t out_len,
                                     const zr_term_state_t* final_ts,
                                     const zr_diff_stats_t* stats) {
  if (!e || !final_ts || !stats) {
    return;
  }

  const uint64_t frame_id_presented = zr_engine_trace_frame_id(e);

  zr_engine_fb_swap(&e->fb_prev, &e->fb_next);
  e->term_state = *final_ts;

  e->metrics.frame_index++;
  e->metrics.bytes_emitted_total += (uint64_t)out_len;
  e->metrics.bytes_emitted_last_frame = (uint32_t)out_len;
  e->metrics.dirty_lines_last_frame = stats->dirty_lines;
  e->metrics.dirty_cols_last_frame = stats->dirty_cells;
  e->metrics.damage_rects_last_frame = stats->damage_rects;
  e->metrics.damage_cells_last_frame = stats->damage_cells;
  e->metrics.damage_full_frame = stats->damage_full_frame;
  e->metrics._pad2[0] = 0u;
  e->metrics._pad2[1] = 0u;
  e->metrics._pad2[2] = 0u;

  /* Update debug trace frame ID and record frame data. */
  if (e->debug_trace) {
    zr_engine_trace_frame(e, frame_id_presented, out_len, stats);
    zr_debug_trace_set_frame(e->debug_trace, zr_engine_trace_frame_id(e));
  }
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

  if (e->cfg_runtime.wait_for_output_drain != 0u) {
    const int32_t timeout_ms = zr_engine_output_wait_timeout_ms(&e->cfg_runtime);
    zr_result_t rc = plat_wait_output_writable(e->plat, timeout_ms);
    if (rc != ZR_OK) {
      return rc;
    }
  }

  size_t out_len = 0u;
  zr_term_state_t final_ts;
  zr_diff_stats_t stats;

  zr_result_t rc = zr_engine_present_render(e, &out_len, &final_ts, &stats);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_engine_present_write(e, out_len);
  if (rc != ZR_OK) {
    return rc;
  }

  zr_engine_present_commit(e, out_len, &final_ts, &stats);
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
    /*
      Resizes must be detectable even when SIGWINCH delivery is disrupted
      (e.g. runtimes that install their own SIGWINCH handlers).

      With timeout_ms==0, plat_wait() may return 0 even if the terminal has
      resized, because there is no wake signal to make poll() ready.

      Fix: always perform a best-effort size check on timeout. This keeps
      non-blocking poll semantics while ensuring resize events are still
      generated and framebuffers resized.
    */
    zr_result_t rc = zr_engine_try_handle_resize(e, time_ms);
    if (rc != ZR_OK) {
      return (int)rc;
    }
    return 0;
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

  uint32_t time_ms = zr_engine_now_ms_u32();

  int wait_ms = timeout_ms;
  if (zr_event_queue_count(&e->evq) == 0u && wait_ms > 0) {
    const uint32_t until_tick_ms = zr_engine_tick_until_due_ms(e, time_ms);
    if (until_tick_ms == 0u) {
      wait_ms = 0;
    } else if (until_tick_ms < (uint32_t)wait_ms) {
      wait_ms = (int)until_tick_ms;
    }
  }

  const int ready = zr_engine_poll_wait_and_fill(e, wait_ms, time_ms);
  if (ready < 0) {
    return ready;
  }

  time_ms = zr_engine_now_ms_u32();
  zr_engine_maybe_enqueue_tick(e, time_ms);

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

zr_result_t engine_get_caps(zr_engine_t* e, zr_terminal_caps_t* out_caps) {
  if (!e || !out_caps) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_terminal_caps_t c;
  memset(&c, 0, sizeof(c));
  c.color_mode = e->caps.color_mode;
  c.supports_mouse = e->caps.supports_mouse;
  c.supports_bracketed_paste = e->caps.supports_bracketed_paste;
  c.supports_focus_events = e->caps.supports_focus_events;
  c.supports_osc52 = e->caps.supports_osc52;
  c.supports_sync_update = e->caps.supports_sync_update;
  c.supports_scroll_region = e->caps.supports_scroll_region;
  c.supports_cursor_shape = e->caps.supports_cursor_shape;
  c.supports_output_wait_writable = e->caps.supports_output_wait_writable;
  c._pad0[0] = 0u;
  c._pad0[1] = 0u;
  c._pad0[2] = 0u;
  c.sgr_attrs_supported = e->caps.sgr_attrs_supported;

  *out_caps = c;
  return ZR_OK;
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

static zr_result_t zr_engine_set_config_prepare_damage_rects(zr_engine_t* e,
                                                            const zr_engine_runtime_config_t* cfg,
                                                            zr_damage_rect_t** out_damage_rects_new,
                                                            uint32_t* out_damage_rect_cap_new,
                                                            bool* want_damage_rects) {
  if (!e || !cfg || !out_damage_rects_new || !out_damage_rect_cap_new || !want_damage_rects) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  *out_damage_rects_new = NULL;
  *out_damage_rect_cap_new = e->damage_rect_cap;
  *want_damage_rects = (cfg->limits.diff_max_damage_rects != e->cfg_runtime.limits.diff_max_damage_rects);

  if (!*want_damage_rects) {
    return ZR_OK;
  }

  const uint32_t cap = cfg->limits.diff_max_damage_rects;
  if (cap == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  size_t bytes = 0u;
  if (!zr_checked_mul_size((size_t)cap, sizeof(zr_damage_rect_t), &bytes)) {
    return ZR_ERR_LIMIT;
  }

  *out_damage_rects_new = (zr_damage_rect_t*)calloc(cap, sizeof(zr_damage_rect_t));
  if (!*out_damage_rects_new) {
    return ZR_ERR_OOM;
  }
  *out_damage_rect_cap_new = cap;
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
                                        bool want_damage_rects,
                                        zr_damage_rect_t** damage_rects_new,
                                        uint32_t damage_rect_cap_new,
                                        bool want_arena_reinit,
                                        zr_arena_t* arena_frame_new,
                                        zr_arena_t* arena_persistent_new) {
  if (!e || !cfg || !out_buf_new || !damage_rects_new || !arena_frame_new || !arena_persistent_new) {
    return;
  }

  if (want_out_buf) {
    free(e->out_buf);
    e->out_buf = *out_buf_new;
    e->out_cap = out_cap_new;
    *out_buf_new = NULL;
  }

  if (want_damage_rects) {
    free(e->damage_rects);
    e->damage_rects = *damage_rects_new;
    e->damage_rect_cap = damage_rect_cap_new;
    *damage_rects_new = NULL;
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
  zr_damage_rect_t* damage_rects_new = NULL;
  uint32_t damage_rect_cap_new = e->damage_rect_cap;
  zr_arena_t arena_frame_new = {0};
  zr_arena_t arena_persistent_new = {0};

  bool want_out_buf = false;
  bool want_damage_rects = false;
  bool want_arena_reinit = false;

  rc = zr_engine_set_config_prepare_out_buf(e, cfg, &out_buf_new, &out_cap_new, &want_out_buf);
  if (rc != ZR_OK) {
    goto cleanup;
  }
  rc = zr_engine_set_config_prepare_damage_rects(e, cfg, &damage_rects_new, &damage_rect_cap_new, &want_damage_rects);
  if (rc != ZR_OK) {
    goto cleanup;
  }
  rc = zr_engine_set_config_prepare_arenas(e, cfg, &arena_frame_new, &arena_persistent_new, &want_arena_reinit);
  if (rc != ZR_OK) {
    goto cleanup;
  }

  /* Commit (no partial effects): allocations succeeded; now swap in new resources. */
  zr_engine_set_config_commit(e, cfg, want_out_buf, &out_buf_new, out_cap_new, want_damage_rects, &damage_rects_new,
                              damage_rect_cap_new, want_arena_reinit, &arena_frame_new, &arena_persistent_new);
  return ZR_OK;

cleanup:
  free(out_buf_new);
  free(damage_rects_new);
  zr_arena_release(&arena_frame_new);
  zr_arena_release(&arena_persistent_new);
  return rc;
}

/* --- Debug Trace API --- */

enum {
  ZR_DEBUG_RING_BUF_SIZE = 256u * 1024u, /* 256 KB for record payloads */
};

/*
  Free all debug trace resources.

  Why: Centralizes cleanup for both disable and destroy paths.
*/
static void zr_engine_debug_free(zr_engine_t* e) {
  if (!e) {
    return;
  }

  free(e->debug_trace);
  e->debug_trace = NULL;

  free(e->debug_ring_buf);
  e->debug_ring_buf = NULL;

  free(e->debug_record_offsets);
  e->debug_record_offsets = NULL;

  free(e->debug_record_sizes);
  e->debug_record_sizes = NULL;
}

zr_result_t engine_debug_enable(zr_engine_t* e, const zr_debug_config_t* config) {
  if (!e) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  /* Free any existing debug trace. */
  zr_engine_debug_free(e);

  zr_debug_config_t cfg = config ? *config : zr_debug_config_default();
  cfg.enabled = 1u;

  const uint32_t ring_cap = (cfg.ring_capacity > 0u) ? cfg.ring_capacity : ZR_DEBUG_DEFAULT_RING_CAP;

  /* Allocate trace context. */
  e->debug_trace = (zr_debug_trace_t*)calloc(1u, sizeof(zr_debug_trace_t));
  if (!e->debug_trace) {
    return ZR_ERR_OOM;
  }

  /* Allocate ring buffer for payloads. */
  e->debug_ring_buf = (uint8_t*)malloc(ZR_DEBUG_RING_BUF_SIZE);
  if (!e->debug_ring_buf) {
    zr_engine_debug_free(e);
    return ZR_ERR_OOM;
  }

  /* Allocate index arrays. */
  e->debug_record_offsets = (uint32_t*)calloc(ring_cap, sizeof(uint32_t));
  if (!e->debug_record_offsets) {
    zr_engine_debug_free(e);
    return ZR_ERR_OOM;
  }

  e->debug_record_sizes = (uint32_t*)calloc(ring_cap, sizeof(uint32_t));
  if (!e->debug_record_sizes) {
    zr_engine_debug_free(e);
    return ZR_ERR_OOM;
  }

  /* Initialize trace context. */
  zr_result_t rc = zr_debug_trace_init(e->debug_trace, &cfg,
                                       e->debug_ring_buf, ZR_DEBUG_RING_BUF_SIZE,
                                       e->debug_record_offsets, e->debug_record_sizes,
                                       ring_cap);
  if (rc != ZR_OK) {
    zr_engine_debug_free(e);
    return rc;
  }

  /* Set start time for relative timestamps. */
  zr_debug_trace_set_start_time(e->debug_trace, (uint64_t)plat_now_ms() * 1000u);
  zr_debug_trace_set_frame(e->debug_trace, zr_engine_trace_frame_id(e));

  return ZR_OK;
}

void engine_debug_disable(zr_engine_t* e) {
  if (!e) {
    return;
  }
  zr_engine_debug_free(e);
}

zr_result_t engine_debug_query(zr_engine_t* e,
                               const zr_debug_query_t* query,
                               zr_debug_record_header_t* out_headers,
                               uint32_t out_headers_cap,
                               zr_debug_query_result_t* out_result) {
  if (!e || !query || !out_result) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  if (!e->debug_trace) {
    memset(out_result, 0, sizeof(*out_result));
    return ZR_OK;
  }

  return zr_debug_trace_query(e->debug_trace, query, out_headers, out_headers_cap, out_result);
}

zr_result_t engine_debug_get_payload(zr_engine_t* e,
                                     uint64_t record_id,
                                     void* out_payload,
                                     uint32_t out_cap,
                                     uint32_t* out_size) {
  if (!e || !out_size) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  *out_size = 0u;

  if (!e->debug_trace) {
    return ZR_ERR_LIMIT;
  }

  return zr_debug_trace_get_payload(e->debug_trace, record_id, out_payload, out_cap, out_size);
}

zr_result_t engine_debug_get_stats(zr_engine_t* e, zr_debug_stats_t* out_stats) {
  if (!e || !out_stats) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  memset(out_stats, 0, sizeof(*out_stats));

  if (!e->debug_trace) {
    return ZR_OK;
  }

  return zr_debug_trace_get_stats(e->debug_trace, out_stats);
}

int32_t engine_debug_export(zr_engine_t* e, uint8_t* out_buf, size_t out_cap) {
  if (!e) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }

  if (!e->debug_trace) {
    return 0;
  }

  return zr_debug_trace_export(e->debug_trace, out_buf, out_cap);
}

void engine_debug_reset(zr_engine_t* e) {
  if (!e || !e->debug_trace) {
    return;
  }
  zr_debug_trace_reset(e->debug_trace);
}
