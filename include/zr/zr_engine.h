/*
  include/zr/zr_engine.h — Public engine ABI surface (header-only declarations).

  Why: Exposes the stable, buffer-oriented API used by wrappers while keeping
  engine allocations owned by the engine and enforcing the platform boundary.
*/

#ifndef ZR_ZR_ENGINE_H_INCLUDED
#define ZR_ZR_ENGINE_H_INCLUDED

#include "zr/zr_config.h"
#include "zr/zr_debug.h"
#include "zr/zr_terminal_caps.h"
#include "zr/zr_metrics.h"
#include "zr/zr_result.h"

#include <stddef.h>
#include <stdint.h>

typedef struct zr_engine_t zr_engine_t;

zr_result_t engine_create(zr_engine_t** out_engine, const zr_engine_config_t* cfg);
void        engine_destroy(zr_engine_t* e);

int         engine_poll_events(zr_engine_t* e, int timeout_ms, uint8_t* out_buf, int out_cap);
zr_result_t engine_post_user_event(zr_engine_t* e, uint32_t tag, const uint8_t* payload, int payload_len);

zr_result_t engine_submit_drawlist(zr_engine_t* e, const uint8_t* bytes, int bytes_len);
zr_result_t engine_present(zr_engine_t* e);

zr_result_t engine_get_metrics(zr_engine_t* e, zr_metrics_t* out_metrics);
zr_result_t engine_get_caps(zr_engine_t* e, zr_terminal_caps_t* out_caps);
zr_result_t engine_set_config(zr_engine_t* e, const zr_engine_runtime_config_t* cfg);

/*
  Debug trace API.

  Why: Enables wrappers to capture and query diagnostic records for debugging
  rendering issues without guessing which subsystem failed.
*/

/*
  engine_debug_enable:
    - Enables debug tracing with the given configuration.
    - Allocates engine-owned storage for trace buffers.
    - May be called multiple times to reconfigure; resets existing traces.
    - Returns ZR_ERR_OOM if allocation fails.
*/
zr_result_t engine_debug_enable(zr_engine_t* e, const zr_debug_config_t* config);

/*
  engine_debug_disable:
    - Disables debug tracing and frees trace buffers.
    - Safe to call even if tracing was never enabled.
*/
void engine_debug_disable(zr_engine_t* e);

/*
  engine_debug_query:
    - Queries debug records matching the filter criteria.
    - out_headers must have space for at least out_headers_cap headers.
    - Returns query statistics in out_result.
*/
zr_result_t engine_debug_query(zr_engine_t* e,
                               const zr_debug_query_t* query,
                               zr_debug_record_header_t* out_headers,
                               uint32_t out_headers_cap,
                               zr_debug_query_result_t* out_result);

/*
  engine_debug_get_payload:
    - Retrieves the payload for a specific record by record_id.
    - Returns ZR_ERR_LIMIT if record not found or doesn't fit in buffer.
*/
zr_result_t engine_debug_get_payload(zr_engine_t* e,
                                     uint64_t record_id,
                                     void* out_payload,
                                     uint32_t out_cap,
                                     uint32_t* out_size);

/*
  engine_debug_get_stats:
    - Returns aggregate debug statistics without querying individual records.
*/
zr_result_t engine_debug_get_stats(zr_engine_t* e, zr_debug_stats_t* out_stats);

/*
  engine_debug_export:
    - Exports all debug records to a caller-provided buffer.
    - Returns bytes written or negative error code.
*/
int32_t engine_debug_export(zr_engine_t* e, uint8_t* out_buf, size_t out_cap);

/*
  engine_debug_reset:
    - Clears all debug records but keeps tracing enabled.
*/
void engine_debug_reset(zr_engine_t* e);

#endif /* ZR_ZR_ENGINE_H_INCLUDED */
