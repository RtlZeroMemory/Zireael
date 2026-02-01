/*
  src/core/zr_metrics.h — ABI-safe engine metrics snapshot (POD, append-only).

  Why: Provides a stable, fixed-width metrics struct for wrappers and internal
  diagnostics. The struct is POD (no pointers) and is designed to be appended
  to over time without breaking older callers that prefix-copy by struct_size.
*/

#ifndef ZR_CORE_ZR_METRICS_H_INCLUDED
#define ZR_CORE_ZR_METRICS_H_INCLUDED

#include <stdint.h>

/*
  zr_metrics_t (ABI):
    - POD: fixed-width integers only; no pointers.
    - Append-only: new fields must be appended at the end.
    - Prefix-copy: callers set out->struct_size to the number of bytes they
      can receive; engine_get_metrics copies only what fits.
*/
typedef struct zr_metrics_t {
  /* --- ABI handshake --- */
  /*
    struct_size:
      - Caller-provided capacity for prefix-copy (bytes).
      - If struct_size != 0, the engine overwrites this field with sizeof(zr_metrics_t)
        in the copied prefix.
  */
  uint32_t struct_size;

  uint32_t negotiated_engine_abi_major;
  uint32_t negotiated_engine_abi_minor;
  uint32_t negotiated_engine_abi_patch;

  uint32_t negotiated_drawlist_version;
  uint32_t negotiated_event_batch_version;

  /* --- Frame and output stats --- */
  uint64_t frame_index; /* increments per present */
  uint32_t fps;         /* best-effort */
  uint32_t _pad0;

  uint64_t bytes_emitted_total;
  uint32_t bytes_emitted_last_frame;
  uint32_t _pad1;

  uint32_t dirty_lines_last_frame;
  uint32_t dirty_cols_last_frame;

  /* --- Timing (microseconds; best-effort telemetry) --- */
  uint32_t us_input_last_frame;
  uint32_t us_drawlist_last_frame;
  uint32_t us_diff_last_frame;
  uint32_t us_write_last_frame;

  /* --- Event stats --- */
  uint32_t events_out_last_poll;
  uint32_t events_dropped_total;

  /* --- Arena high-water marks (bytes) --- */
  uint64_t arena_frame_high_water_bytes;
  uint64_t arena_persistent_high_water_bytes;
} zr_metrics_t;

#define ZR_METRICS_STATIC_ASSERT_U32_(field)                                                     \
  _Static_assert(_Generic(((zr_metrics_t*)0)->field, uint32_t : 1, default : 0),                  \
                 "zr_metrics_t field must be uint32_t")

#define ZR_METRICS_STATIC_ASSERT_U64_(field)                                                     \
  _Static_assert(_Generic(((zr_metrics_t*)0)->field, uint64_t : 1, default : 0),                  \
                 "zr_metrics_t field must be uint64_t")

/* Compile-time "no pointers" enforcement: every field must be a fixed-width int. */
ZR_METRICS_STATIC_ASSERT_U32_(struct_size);
ZR_METRICS_STATIC_ASSERT_U32_(negotiated_engine_abi_major);
ZR_METRICS_STATIC_ASSERT_U32_(negotiated_engine_abi_minor);
ZR_METRICS_STATIC_ASSERT_U32_(negotiated_engine_abi_patch);
ZR_METRICS_STATIC_ASSERT_U32_(negotiated_drawlist_version);
ZR_METRICS_STATIC_ASSERT_U32_(negotiated_event_batch_version);
ZR_METRICS_STATIC_ASSERT_U64_(frame_index);
ZR_METRICS_STATIC_ASSERT_U32_(fps);
ZR_METRICS_STATIC_ASSERT_U32_(_pad0);
ZR_METRICS_STATIC_ASSERT_U64_(bytes_emitted_total);
ZR_METRICS_STATIC_ASSERT_U32_(bytes_emitted_last_frame);
ZR_METRICS_STATIC_ASSERT_U32_(_pad1);
ZR_METRICS_STATIC_ASSERT_U32_(dirty_lines_last_frame);
ZR_METRICS_STATIC_ASSERT_U32_(dirty_cols_last_frame);
ZR_METRICS_STATIC_ASSERT_U32_(us_input_last_frame);
ZR_METRICS_STATIC_ASSERT_U32_(us_drawlist_last_frame);
ZR_METRICS_STATIC_ASSERT_U32_(us_diff_last_frame);
ZR_METRICS_STATIC_ASSERT_U32_(us_write_last_frame);
ZR_METRICS_STATIC_ASSERT_U32_(events_out_last_poll);
ZR_METRICS_STATIC_ASSERT_U32_(events_dropped_total);
ZR_METRICS_STATIC_ASSERT_U64_(arena_frame_high_water_bytes);
ZR_METRICS_STATIC_ASSERT_U64_(arena_persistent_high_water_bytes);

#undef ZR_METRICS_STATIC_ASSERT_U32_
#undef ZR_METRICS_STATIC_ASSERT_U64_

#endif /* ZR_CORE_ZR_METRICS_H_INCLUDED */
