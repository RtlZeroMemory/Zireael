/*
  src/core/zr_engine.h — Public engine ABI surface (header-only declarations).

  Why: Exposes the stable, buffer-oriented API used by wrappers while keeping
  engine allocations owned by the engine and enforcing the platform boundary.

  Threading:
    - engine_post_user_event() is thread-safe and may be called from any thread.
    - All other engine_* calls are engine-thread only.

  Ownership:
    - The caller provides drawlist bytes and packed event output buffers.
    - The engine owns all allocations it makes; callers never free engine memory.
    - The engine does not retain pointers into caller buffers beyond a call.

  Errors:
    - ZR_OK == 0 on success; negative ZR_ERR_* on failure.
    - Event batch truncation is reserved as a success mode (see
      docs/ERROR_CODES_CATALOG.md and docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md). When truncated,
      engine_poll_events() returns the bytes written and the batch header has ZR_EV_BATCH_TRUNCATED set.

  engine_poll_events return convention:
    - > 0: bytes written to out_buf
    -   0: no events before timeout_ms
    - < 0: failure (negative ZR_ERR_*)
*/

#ifndef ZR_CORE_ZR_ENGINE_H_INCLUDED
#define ZR_CORE_ZR_ENGINE_H_INCLUDED

#include "core/zr_config.h"
#include "core/zr_metrics.h"

#include "util/zr_result.h"

#include <stdint.h>

typedef struct zr_engine_t zr_engine_t;

zr_result_t engine_create(zr_engine_t** out_engine, const zr_engine_config_t* cfg);
void        engine_destroy(zr_engine_t* e);

int         engine_poll_events(zr_engine_t* e, int timeout_ms, uint8_t* out_buf, int out_cap);
zr_result_t engine_post_user_event(zr_engine_t* e, uint32_t tag, const uint8_t* payload, int payload_len);

zr_result_t engine_submit_drawlist(zr_engine_t* e, const uint8_t* bytes, int bytes_len);
zr_result_t engine_present(zr_engine_t* e);

zr_result_t engine_get_metrics(zr_engine_t* e, zr_metrics_t* out_metrics);
zr_result_t engine_set_config(zr_engine_t* e, const zr_engine_runtime_config_t* cfg);

#endif /* ZR_CORE_ZR_ENGINE_H_INCLUDED */
