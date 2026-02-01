/*
  src/core/zr_engine_stub.c — Minimal stubs for the public engine ABI.

  Why: Keeps the library linkable while the real engine wiring is implemented
  in later tasks, without exposing OS headers or internal state.
*/

#include "core/zr_engine.h"

zr_result_t engine_create(zr_engine_t** out_engine, const zr_engine_config_t* cfg) {
  if (!out_engine || !cfg) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_engine = NULL;
  return ZR_ERR_UNSUPPORTED;
}

void engine_destroy(zr_engine_t* e) { (void)e; }

int engine_poll_events(zr_engine_t* e, int timeout_ms, uint8_t* out_buf, int out_cap) {
  (void)timeout_ms;
  if (!e) {
    return (int)ZR_ERR_INVALID_ARGUMENT;
  }
  if (out_cap < 0) {
    return (int)ZR_ERR_INVALID_ARGUMENT;
  }
  if (out_cap > 0 && !out_buf) {
    return (int)ZR_ERR_INVALID_ARGUMENT;
  }
  return (int)ZR_ERR_UNSUPPORTED;
}

zr_result_t engine_post_user_event(zr_engine_t* e, uint32_t tag, const uint8_t* payload, int payload_len) {
  (void)tag;
  if (!e) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (payload_len < 0) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (payload_len > 0 && !payload) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return ZR_ERR_UNSUPPORTED;
}

zr_result_t engine_submit_drawlist(zr_engine_t* e, const uint8_t* bytes, int bytes_len) {
  if (!e || !bytes) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (bytes_len < 0) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return ZR_ERR_UNSUPPORTED;
}

zr_result_t engine_present(zr_engine_t* e) {
  if (!e) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return ZR_ERR_UNSUPPORTED;
}

zr_result_t engine_get_metrics(zr_engine_t* e, zr_metrics_t* out_metrics) {
  if (!e || !out_metrics) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return ZR_ERR_UNSUPPORTED;
}

zr_result_t engine_set_config(zr_engine_t* e, const zr_engine_runtime_config_t* cfg) {
  if (!e || !cfg) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return ZR_ERR_UNSUPPORTED;
}

