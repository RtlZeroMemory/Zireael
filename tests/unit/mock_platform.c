/*
  tests/unit/mock_platform.c — OS-header-free mock platform backend.

  Why: Implements the platform boundary symbols (plat_*) for unit tests so
  engine code can be exercised deterministically without linking OS backends.
*/

#include "unit/mock_platform.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

enum {
  ZR_MOCK_INPUT_CAP = 16u * 1024u,
  ZR_MOCK_WRITE_CAPTURE_CAP = 32u * 1024u,
};

typedef struct plat_t {
  bool created;
  bool raw;

  plat_config_t cfg;
  plat_caps_t caps;
  plat_size_t size;

  uint8_t input[ZR_MOCK_INPUT_CAP];
  size_t input_len;
  size_t input_off;
  uint32_t read_max;

  uint8_t write_last[ZR_MOCK_WRITE_CAPTURE_CAP];
  size_t write_last_len;
  uint64_t write_total_len;
  uint32_t write_calls;

  bool wake_pending;
  uint32_t wake_calls;

  bool output_writable;
  uint32_t wait_output_calls;

  uint64_t now_ms;
} plat_t;

static plat_t g_plat;

static void zr_mock_plat_default_caps(plat_caps_t* out) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->color_mode = PLAT_COLOR_MODE_RGB;
  out->supports_mouse = 1u;
  out->supports_bracketed_paste = 1u;
  out->supports_focus_events = 0u;
  out->supports_osc52 = 0u;
  out->supports_sync_update = 0u;
  out->supports_scroll_region = 1u;
  out->supports_cursor_shape = 1u;
  out->supports_output_wait_writable = 1u;
  out->_pad0[0] = 0u;
  out->_pad0[1] = 0u;
  out->_pad0[2] = 0u;

  /* Allow all style attrs in unit tests. */
  out->sgr_attrs_supported = 0xFFFFFFFFu;
}

void mock_plat_reset(void) {
  memset(&g_plat, 0, sizeof(g_plat));
  g_plat.size.cols = 80u;
  g_plat.size.rows = 24u;
  g_plat.read_max = 0u;
  g_plat.output_writable = true;
  g_plat.wait_output_calls = 0u;
  zr_mock_plat_default_caps(&g_plat.caps);
}

void mock_plat_set_size(uint32_t cols, uint32_t rows) {
  g_plat.size.cols = cols;
  g_plat.size.rows = rows;
}

void mock_plat_set_caps(plat_caps_t caps) {
  g_plat.caps = caps;
}

void mock_plat_set_now_ms(uint64_t now_ms) {
  g_plat.now_ms = now_ms;
}

void mock_plat_set_output_writable(uint8_t writable) {
  g_plat.output_writable = (writable != 0u);
}

void mock_plat_set_read_max(uint32_t max_bytes) {
  g_plat.read_max = max_bytes;
}

zr_result_t mock_plat_push_input(const uint8_t* bytes, size_t len) {
  if (!bytes && len != 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (len > (ZR_MOCK_INPUT_CAP - g_plat.input_len)) {
    return ZR_ERR_LIMIT;
  }
  if (len != 0u) {
    memcpy(g_plat.input + g_plat.input_len, bytes, len);
    g_plat.input_len += len;
  }
  return ZR_OK;
}

void mock_plat_clear_writes(void) {
  g_plat.write_last_len = 0u;
  g_plat.write_total_len = 0u;
  g_plat.write_calls = 0u;
}

uint32_t mock_plat_write_call_count(void) {
  return g_plat.write_calls;
}

uint32_t mock_plat_wait_output_call_count(void) {
  return g_plat.wait_output_calls;
}

uint64_t mock_plat_bytes_written_total(void) {
  return g_plat.write_total_len;
}

size_t mock_plat_last_write_len(void) {
  return g_plat.write_last_len;
}

size_t mock_plat_last_write_copy(uint8_t* out, size_t out_cap) {
  if (!out && out_cap != 0u) {
    return 0u;
  }
  size_t n = g_plat.write_last_len;
  if (n > out_cap) {
    n = out_cap;
  }
  if (n != 0u) {
    memcpy(out, g_plat.write_last, n);
  }
  return n;
}

zr_result_t plat_create(plat_t** out_plat, const plat_config_t* cfg) {
  if (!out_plat || !cfg) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_plat = NULL;

  if (g_plat.created) {
    return ZR_ERR_PLATFORM;
  }

  g_plat.created = true;
  g_plat.raw = false;
  g_plat.cfg = *cfg;
  g_plat.input_len = 0u;
  g_plat.input_off = 0u;
  g_plat.wake_pending = false;
  g_plat.wake_calls = 0u;
  mock_plat_clear_writes();

  *out_plat = &g_plat;
  return ZR_OK;
}

void plat_destroy(plat_t* plat) {
  if (!plat) {
    return;
  }
  plat->created = false;
  plat->raw = false;
}

zr_result_t plat_enter_raw(plat_t* plat) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  plat->raw = true;
  return ZR_OK;
}

zr_result_t plat_leave_raw(plat_t* plat) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  plat->raw = false;
  return ZR_OK;
}

zr_result_t plat_get_size(plat_t* plat, plat_size_t* out_size) {
  if (!plat || !out_size) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_size = plat->size;
  return ZR_OK;
}

zr_result_t plat_get_caps(plat_t* plat, plat_caps_t* out_caps) {
  if (!plat || !out_caps) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_caps = plat->caps;
  return ZR_OK;
}

int32_t plat_read_input(plat_t* plat, uint8_t* out_buf, int32_t out_cap) {
  if (!plat) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }
  if (out_cap < 0) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }
  if (out_cap != 0 && !out_buf) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }

  const size_t avail = (plat->input_len >= plat->input_off) ? (plat->input_len - plat->input_off) : 0u;
  if (avail == 0u || out_cap == 0) {
    return 0;
  }

  size_t n = (size_t)out_cap;
  if (n > avail) {
    n = avail;
  }

  if (plat->read_max != 0u && n > (size_t)plat->read_max) {
    n = (size_t)plat->read_max;
  }

  memcpy(out_buf, plat->input + plat->input_off, n);
  plat->input_off += n;
  if (plat->input_off == plat->input_len) {
    plat->input_off = 0u;
    plat->input_len = 0u;
  }

  if (n > (size_t)INT32_MAX) {
    return (int32_t)ZR_ERR_LIMIT;
  }
  return (int32_t)n;
}

zr_result_t plat_write_output(plat_t* plat, const uint8_t* bytes, int32_t len) {
  if (!plat || (!bytes && len != 0)) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (len < 0) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  plat->write_calls++;
  plat->write_total_len += (uint64_t)len;

  plat->write_last_len = 0u;
  if (len != 0) {
    size_t n = (size_t)len;
    if (n > ZR_MOCK_WRITE_CAPTURE_CAP) {
      n = ZR_MOCK_WRITE_CAPTURE_CAP;
    }
    memcpy(plat->write_last, bytes, n);
    plat->write_last_len = n;
  }

  return ZR_OK;
}

zr_result_t plat_wait_output_writable(plat_t* plat, int32_t timeout_ms) {
  (void)timeout_ms;
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  plat->wait_output_calls++;
  if (plat->caps.supports_output_wait_writable == 0u) {
    return ZR_ERR_UNSUPPORTED;
  }
  return plat->output_writable ? ZR_OK : ZR_ERR_LIMIT;
}

int32_t plat_wait(plat_t* plat, int32_t timeout_ms) {
  (void)timeout_ms;
  if (!plat) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }

  if (plat->wake_pending) {
    plat->wake_pending = false;
    return 1;
  }

  if (plat->input_len != 0u) {
    return 1;
  }
  return 0;
}

zr_result_t plat_wake(plat_t* plat) {
  if (!plat) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  plat->wake_pending = true;
  plat->wake_calls++;
  return ZR_OK;
}

uint64_t plat_now_ms(void) {
  return g_plat.now_ms;
}
