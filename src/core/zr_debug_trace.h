/*
  src/core/zr_debug_trace.h — Internal debug trace ring buffer and record management.

  Why: Provides a deterministic, bounded trace buffer for capturing diagnostic
  records without per-frame heap allocations. The ring buffer uses caller-provided
  storage and overwrites oldest records when full.

  Thread-safety:
    - All functions must be called from the engine thread only.
    - No internal locking; engine thread affinity is enforced by contract.
*/

#ifndef ZR_CORE_ZR_DEBUG_TRACE_H_INCLUDED
#define ZR_CORE_ZR_DEBUG_TRACE_H_INCLUDED

#include "zr/zr_debug.h"
#include "util/zr_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
  Default ring buffer capacity (records, not bytes).

  Why: Provides enough history for typical debugging without excessive memory.
*/
enum { ZR_DEBUG_DEFAULT_RING_CAP = 1024u };

/*
  Maximum payload size for variable-length records.

  Why: Bounds memory usage and simplifies ring buffer management.
*/
enum { ZR_DEBUG_MAX_PAYLOAD_SIZE = 4096u };

/*
  Debug record codes for each category.

  Why: Provides machine-readable event types for programmatic analysis.
*/
typedef enum zr_debug_code_t {
  /* Frame codes (ZR_DEBUG_CAT_FRAME) */
  ZR_DEBUG_CODE_FRAME_BEGIN = 0x0100,
  ZR_DEBUG_CODE_FRAME_SUBMIT = 0x0101,
  ZR_DEBUG_CODE_FRAME_PRESENT = 0x0102,
  ZR_DEBUG_CODE_FRAME_RESIZE = 0x0103,

  /* Event codes (ZR_DEBUG_CAT_EVENT) */
  ZR_DEBUG_CODE_EVENT_POLL_BEGIN = 0x0200,
  ZR_DEBUG_CODE_EVENT_POLL_END = 0x0201,
  ZR_DEBUG_CODE_EVENT_PARSED = 0x0202,
  ZR_DEBUG_CODE_EVENT_DROPPED = 0x0203,

  /* Drawlist codes (ZR_DEBUG_CAT_DRAWLIST) */
  ZR_DEBUG_CODE_DRAWLIST_VALIDATE = 0x0300,
  ZR_DEBUG_CODE_DRAWLIST_EXECUTE = 0x0301,
  ZR_DEBUG_CODE_DRAWLIST_CMD = 0x0302,

  /* Error codes (ZR_DEBUG_CAT_ERROR) */
  ZR_DEBUG_CODE_ERROR_GENERIC = 0x0400,
  ZR_DEBUG_CODE_ERROR_DRAWLIST = 0x0401,
  ZR_DEBUG_CODE_ERROR_EVENT = 0x0402,
  ZR_DEBUG_CODE_ERROR_PLATFORM = 0x0403,

  /* State codes (ZR_DEBUG_CAT_STATE) */
  ZR_DEBUG_CODE_STATE_CHANGE = 0x0500,

  /* Perf codes (ZR_DEBUG_CAT_PERF) */
  ZR_DEBUG_CODE_PERF_TIMING = 0x0600,
} zr_debug_code_t;

/*
  Debug trace context.

  Why: Encapsulates all state needed for trace capture without exposing internals.

  Memory layout:
    - Fixed header fields for bookkeeping.
    - Ring buffer storage is caller-provided via init.
*/
typedef struct zr_debug_trace_t {
  /* --- Configuration --- */
  zr_debug_config_t config;

  /* --- Ring buffer storage (caller-owned) --- */
  uint8_t* ring_buf;            /* Raw byte storage */
  size_t ring_buf_cap;          /* Total byte capacity */

  /* --- Record index ring (caller-owned) --- */
  /*
    Why: Separate index allows O(1) record lookup by slot without scanning
    variable-length payloads.
  */
  uint32_t* record_offsets;     /* Offset into ring_buf for each record */
  uint32_t* record_sizes;       /* Size of each record (header + payload) */
  uint32_t index_cap;           /* Number of index slots */
  uint32_t index_head;          /* Next slot to write */
  uint32_t index_count;         /* Current number of valid records */

  /* --- Counters --- */
  uint64_t next_record_id;
  uint64_t total_dropped;
  uint64_t current_frame_id;
  uint64_t start_time_us;       /* Engine creation time for relative timestamps */

  /* --- Aggregated stats --- */
  uint32_t error_count;
  uint32_t warn_count;

  /* --- Byte ring state --- */
  size_t byte_head;             /* Next write position */
  size_t byte_used;             /* Bytes currently in use */
} zr_debug_trace_t;

/*
  zr_debug_trace_init:
    - Initializes the trace context with caller-provided storage.
    - ring_buf must be at least (ring_cap * avg_record_size) bytes.
    - record_offsets and record_sizes must each have ring_cap entries.
    - Returns ZR_ERR_INVALID_ARGUMENT if any pointer is NULL or capacity is 0.
*/
zr_result_t zr_debug_trace_init(zr_debug_trace_t* t,
                                const zr_debug_config_t* config,
                                uint8_t* ring_buf,
                                size_t ring_buf_cap,
                                uint32_t* record_offsets,
                                uint32_t* record_sizes,
                                uint32_t index_cap);

/*
  zr_debug_trace_reset:
    - Clears all records but preserves configuration and storage pointers.
    - Resets counters to initial state.
*/
void zr_debug_trace_reset(zr_debug_trace_t* t);

/*
  zr_debug_trace_set_frame:
    - Sets the current frame ID for subsequent records.
    - Called by engine at frame boundaries.
*/
void zr_debug_trace_set_frame(zr_debug_trace_t* t, uint64_t frame_id);

/*
  zr_debug_trace_set_start_time:
    - Sets the engine start time for computing relative timestamps.
*/
void zr_debug_trace_set_start_time(zr_debug_trace_t* t, uint64_t start_time_us);

/*
  zr_debug_trace_enabled:
    - Returns true if tracing is enabled for the given category and severity.
*/
bool zr_debug_trace_enabled(const zr_debug_trace_t* t,
                            zr_debug_category_t category,
                            zr_debug_severity_t severity);

/*
  zr_debug_trace_record:
    - Appends a record to the trace buffer with timestamp.
    - timestamp_us is absolute microseconds (will be converted to relative).
    - Overwrites oldest records if the ring is full.
    - payload may be NULL if payload_size is 0.
    - Returns ZR_OK on success, ZR_ERR_LIMIT if payload exceeds max size.
*/
zr_result_t zr_debug_trace_record(zr_debug_trace_t* t,
                                  zr_debug_category_t category,
                                  zr_debug_severity_t severity,
                                  uint32_t code,
                                  uint64_t timestamp_us,
                                  const void* payload,
                                  uint32_t payload_size);

/*
  Convenience wrappers for common record types.
  All take timestamp_us as absolute microseconds.
*/
zr_result_t zr_debug_trace_frame(zr_debug_trace_t* t,
                                 uint32_t code,
                                 uint64_t timestamp_us,
                                 const zr_debug_frame_record_t* frame);

zr_result_t zr_debug_trace_event(zr_debug_trace_t* t,
                                 uint32_t code,
                                 zr_debug_severity_t severity,
                                 uint64_t timestamp_us,
                                 const zr_debug_event_record_t* event);

zr_result_t zr_debug_trace_error(zr_debug_trace_t* t,
                                 uint32_t code,
                                 uint64_t timestamp_us,
                                 const zr_debug_error_record_t* error);

zr_result_t zr_debug_trace_drawlist(zr_debug_trace_t* t,
                                    uint32_t code,
                                    uint64_t timestamp_us,
                                    const zr_debug_drawlist_record_t* dl);

zr_result_t zr_debug_trace_perf(zr_debug_trace_t* t,
                                uint64_t timestamp_us,
                                const zr_debug_perf_record_t* perf);

/*
  zr_debug_trace_query:
    - Queries records matching the filter criteria.
    - out_headers must have space for at least query->max_records headers.
    - Returns query statistics in out_result.
*/
zr_result_t zr_debug_trace_query(const zr_debug_trace_t* t,
                                 const zr_debug_query_t* query,
                                 zr_debug_record_header_t* out_headers,
                                 uint32_t out_headers_cap,
                                 zr_debug_query_result_t* out_result);

/*
  zr_debug_trace_get_payload:
    - Retrieves the payload for a record by record_id.
    - out_payload must have at least out_cap bytes.
    - Returns ZR_ERR_LIMIT if record not found or payload doesn't fit.
*/
zr_result_t zr_debug_trace_get_payload(const zr_debug_trace_t* t,
                                       uint64_t record_id,
                                       void* out_payload,
                                       uint32_t out_cap,
                                       uint32_t* out_size);

/*
  zr_debug_trace_get_stats:
    - Returns aggregate statistics without querying individual records.
*/
zr_result_t zr_debug_trace_get_stats(const zr_debug_trace_t* t,
                                     zr_debug_stats_t* out_stats);

/*
  zr_debug_trace_export:
    - Exports all records to a caller-provided buffer.
    - Format: sequence of (header, payload) pairs.
    - Returns bytes written or negative error code.
*/
int32_t zr_debug_trace_export(const zr_debug_trace_t* t,
                              uint8_t* out_buf,
                              size_t out_cap);

#endif /* ZR_CORE_ZR_DEBUG_TRACE_H_INCLUDED */
