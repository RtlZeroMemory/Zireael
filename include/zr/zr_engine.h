/*
  include/zr/zr_engine.h — Public engine ABI surface (header-only declarations).

  Why: Exposes the stable, buffer-oriented API used by wrappers while keeping
  engine allocations owned by the engine and enforcing the platform boundary.
*/

#ifndef ZR_ZR_ENGINE_H_INCLUDED
#define ZR_ZR_ENGINE_H_INCLUDED

#include "zr/zr_config.h"
#include "zr/zr_metrics.h"
#include "zr/zr_result.h"

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

#endif /* ZR_ZR_ENGINE_H_INCLUDED */
