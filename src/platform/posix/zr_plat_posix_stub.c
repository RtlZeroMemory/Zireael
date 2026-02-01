/*
  src/platform/posix/zr_plat_posix_stub.c — Temporary POSIX backend stub.

  Why: Keeps the build green until the real POSIX backend lands (EPIC-007).
*/

#include "platform/zr_platform.h"

zr_result_t zr_plat_posix_create(plat_t** out_plat, const plat_config_t* cfg) {
  if (out_plat) {
    *out_plat = NULL;
  }
  (void)cfg;
  return ZR_ERR_UNSUPPORTED;
}

void plat_destroy(plat_t* plat) {
  (void)plat;
}

zr_result_t plat_enter_raw(plat_t* plat) {
  (void)plat;
  return ZR_ERR_UNSUPPORTED;
}

zr_result_t plat_leave_raw(plat_t* plat) {
  (void)plat;
  return ZR_ERR_UNSUPPORTED;
}

zr_result_t plat_get_size(plat_t* plat, plat_size_t* out_size) {
  (void)plat;
  if (!out_size) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  out_size->cols = 0u;
  out_size->rows = 0u;
  return ZR_ERR_UNSUPPORTED;
}

zr_result_t plat_get_caps(plat_t* plat, plat_caps_t* out_caps) {
  (void)plat;
  if (!out_caps) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  out_caps->color_mode = PLAT_COLOR_MODE_UNKNOWN;
  out_caps->supports_mouse = 0u;
  out_caps->supports_bracketed_paste = 0u;
  out_caps->supports_focus_events = 0u;
  out_caps->supports_osc52 = 0u;
  out_caps->_pad[0] = 0u;
  out_caps->_pad[1] = 0u;
  out_caps->_pad[2] = 0u;
  return ZR_ERR_UNSUPPORTED;
}

int32_t plat_read_input(plat_t* plat, uint8_t* out_buf, int32_t out_cap) {
  (void)plat;
  (void)out_buf;
  (void)out_cap;
  return (int32_t)ZR_ERR_UNSUPPORTED;
}

zr_result_t plat_write_output(plat_t* plat, const uint8_t* bytes, int32_t len) {
  (void)plat;
  (void)bytes;
  (void)len;
  return ZR_ERR_UNSUPPORTED;
}

int32_t plat_wait(plat_t* plat, int32_t timeout_ms) {
  (void)plat;
  (void)timeout_ms;
  return (int32_t)ZR_ERR_UNSUPPORTED;
}

zr_result_t plat_wake(plat_t* plat) {
  (void)plat;
  return ZR_ERR_UNSUPPORTED;
}

uint64_t plat_now_ms(void) {
  return 0ull;
}
