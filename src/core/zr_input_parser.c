/*
  src/core/zr_input_parser.c — Deterministic input byte parser implementation.

  Why: Provides a bounded parser for common VT/xterm input sequences that
  stays deterministic under malformed input and never hangs on arbitrary bytes.
*/

#include "core/zr_input_parser.h"

#include "unicode/zr_utf8.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

enum {
  ZR_IN_ESC = 0x1Bu,
  ZR_IN_DEL = 0x7Fu,
};

static void zr__push_key(zr_event_queue_t* q, uint32_t time_ms, zr_key_t key, uint32_t mods) {
  zr_event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ZR_EV_KEY;
  ev.time_ms = time_ms;
  ev.flags = 0u;
  ev.u.key.key = (uint32_t)key;
  ev.u.key.mods = mods;
  ev.u.key.action = (uint32_t)ZR_KEY_ACTION_DOWN;
  ev.u.key.reserved0 = 0u;
  (void)zr_event_queue_push(q, &ev);
}

static void zr__push_text_scalar(zr_event_queue_t* q, uint32_t time_ms, uint32_t scalar) {
  zr_event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ZR_EV_TEXT;
  ev.time_ms = time_ms;
  ev.flags = 0u;
  ev.u.text.codepoint = scalar;
  ev.u.text.reserved0 = 0u;
  (void)zr_event_queue_push(q, &ev);
}

static void zr__push_mouse(zr_event_queue_t* q, uint32_t time_ms, int32_t x, int32_t y, uint32_t kind, uint32_t mods,
                           uint32_t buttons, int32_t wheel_x, int32_t wheel_y) {
  zr_event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ZR_EV_MOUSE;
  ev.time_ms = time_ms;
  ev.flags = 0u;
  ev.u.mouse.x = x;
  ev.u.mouse.y = y;
  ev.u.mouse.kind = kind;
  ev.u.mouse.mods = mods;
  ev.u.mouse.buttons = buttons;
  ev.u.mouse.wheel_x = wheel_x;
  ev.u.mouse.wheel_y = wheel_y;
  ev.u.mouse.reserved0 = 0u;
  (void)zr_event_queue_push(q, &ev);
}

static bool zr__is_digit(uint8_t b) { return b >= (uint8_t)'0' && b <= (uint8_t)'9'; }

static bool zr__parse_u32_dec(const uint8_t* bytes, size_t len, size_t* io_i, uint32_t* out_val) {
  if (!bytes || !io_i || !out_val) {
    return false;
  }

  size_t i = *io_i;
  if (i >= len || !zr__is_digit(bytes[i])) {
    return false;
  }

  uint32_t v = 0u;
  while (i < len && zr__is_digit(bytes[i])) {
    const uint32_t d = (uint32_t)(bytes[i] - (uint8_t)'0');
    if (v > (UINT32_MAX / 10u)) {
      return false;
    }
    if (v == (UINT32_MAX / 10u) && d > (UINT32_MAX % 10u)) {
      return false;
    }
    v = (v * 10u) + d;
    i++;
  }

  *io_i = i;
  *out_val = v;
  return true;
}

static uint32_t zr__mods_from_csi_param(uint32_t p) {
  switch (p) {
    case 2u:
      return ZR_MOD_SHIFT;
    case 3u:
      return ZR_MOD_ALT;
    case 4u:
      return ZR_MOD_SHIFT | ZR_MOD_ALT;
    case 5u:
      return ZR_MOD_CTRL;
    case 6u:
      return ZR_MOD_SHIFT | ZR_MOD_CTRL;
    case 7u:
      return ZR_MOD_ALT | ZR_MOD_CTRL;
    case 8u:
      return ZR_MOD_SHIFT | ZR_MOD_ALT | ZR_MOD_CTRL;
    default:
      return 0u;
  }
}

static bool zr__parse_csi_paste_marker(const uint8_t* bytes, size_t len, size_t i, size_t* out_consumed) {
  if (!bytes || !out_consumed) {
    return false;
  }
  if ((i + 2u) >= len) {
    return false;
  }
  if (bytes[i] != ZR_IN_ESC || bytes[i + 1u] != (uint8_t)'[') {
    return false;
  }

  size_t j = i + 2u;
  uint32_t marker = 0u;
  if (!zr__parse_u32_dec(bytes, len, &j, &marker)) {
    return false;
  }
  if (j >= len || bytes[j] != (uint8_t)'~') {
    return false;
  }
  if (marker != 200u && marker != 201u) {
    return false;
  }

  *out_consumed = (j + 1u) - i;
  return true;
}

static bool zr__parse_csi_tilde_key(const uint8_t* bytes, size_t len, size_t i, zr_key_t* out_key,
                                    uint32_t* out_mods, size_t* out_consumed) {
  if (!bytes || !out_key || !out_mods || !out_consumed) {
    return false;
  }
  if ((i + 2u) >= len) {
    return false;
  }
  if (bytes[i] != ZR_IN_ESC || bytes[i + 1u] != (uint8_t)'[') {
    return false;
  }

  size_t j = i + 2u;
  uint32_t first = 0u;
  if (!zr__parse_u32_dec(bytes, len, &j, &first)) {
    return false;
  }

  uint32_t second = 0u;
  bool have_second = false;

  while (j < len && bytes[j] != (uint8_t)'~') {
    if (bytes[j] != (uint8_t)';') {
      return false;
    }
    j++;
    uint32_t p = 0u;
    if (!zr__parse_u32_dec(bytes, len, &j, &p)) {
      return false;
    }
    if (!have_second) {
      second = p;
      have_second = true;
    }
  }

  if (j >= len || bytes[j] != (uint8_t)'~') {
    return false;
  }

  zr_key_t key = ZR_KEY_UNKNOWN;
  switch (first) {
    case 1u:
    case 7u:
      key = ZR_KEY_HOME;
      break;
    case 4u:
    case 8u:
      key = ZR_KEY_END;
      break;
    case 2u:
      key = ZR_KEY_INSERT;
      break;
    case 3u:
      key = ZR_KEY_DELETE;
      break;
    case 5u:
      key = ZR_KEY_PAGE_UP;
      break;
    case 6u:
      key = ZR_KEY_PAGE_DOWN;
      break;
    case 15u:
      key = ZR_KEY_F5;
      break;
    case 17u:
      key = ZR_KEY_F6;
      break;
    case 18u:
      key = ZR_KEY_F7;
      break;
    case 19u:
      key = ZR_KEY_F8;
      break;
    case 20u:
      key = ZR_KEY_F9;
      break;
    case 21u:
      key = ZR_KEY_F10;
      break;
    case 23u:
      key = ZR_KEY_F11;
      break;
    case 24u:
      key = ZR_KEY_F12;
      break;
    default:
      return false;
  }

  *out_key = key;
  *out_mods = have_second ? zr__mods_from_csi_param(second) : 0u;
  *out_consumed = (j + 1u) - i;
  return true;
}

static bool zr__parse_csi_simple_key(const uint8_t* bytes, size_t len, size_t i, zr_key_t* out_key,
                                     uint32_t* out_mods, size_t* out_consumed) {
  if (!bytes || !out_key || !out_mods || !out_consumed) {
    return false;
  }
  if ((i + 2u) >= len) {
    return false;
  }
  if (bytes[i] != ZR_IN_ESC || bytes[i + 1u] != (uint8_t)'[') {
    return false;
  }

  size_t j = i + 2u;
  uint32_t params[2] = {0u, 0u};
  uint32_t param_count = 0u;

  while (j < len && (zr__is_digit(bytes[j]) || bytes[j] == (uint8_t)';')) {
    if (param_count >= 2u) {
      return false;
    }
    uint32_t v = 0u;
    if (!zr__parse_u32_dec(bytes, len, &j, &v)) {
      return false;
    }
    params[param_count++] = v;
    if (j < len && bytes[j] == (uint8_t)';') {
      j++;
    }
  }

  if (j >= len) {
    return false;
  }

  zr_key_t key = ZR_KEY_UNKNOWN;
  switch (bytes[j]) {
    case (uint8_t)'A':
      key = ZR_KEY_UP;
      break;
    case (uint8_t)'B':
      key = ZR_KEY_DOWN;
      break;
    case (uint8_t)'C':
      key = ZR_KEY_RIGHT;
      break;
    case (uint8_t)'D':
      key = ZR_KEY_LEFT;
      break;
    case (uint8_t)'H':
      key = ZR_KEY_HOME;
      break;
    case (uint8_t)'F':
      key = ZR_KEY_END;
      break;
    default:
      return false;
  }

  *out_key = key;
  *out_mods = (param_count >= 2u) ? zr__mods_from_csi_param(params[1]) : 0u;
  *out_consumed = (j + 1u) - i;
  return true;
}

static bool zr__parse_ss3_key(const uint8_t* bytes, size_t len, size_t i, zr_key_t* out_key, size_t* out_consumed) {
  if (!bytes || !out_key || !out_consumed) {
    return false;
  }
  if ((i + 2u) >= len) {
    return false;
  }
  if (bytes[i] != ZR_IN_ESC || bytes[i + 1u] != (uint8_t)'O') {
    return false;
  }

  zr_key_t key = ZR_KEY_UNKNOWN;
  switch (bytes[i + 2u]) {
    case (uint8_t)'A':
      key = ZR_KEY_UP;
      break;
    case (uint8_t)'B':
      key = ZR_KEY_DOWN;
      break;
    case (uint8_t)'C':
      key = ZR_KEY_RIGHT;
      break;
    case (uint8_t)'D':
      key = ZR_KEY_LEFT;
      break;
    case (uint8_t)'H':
      key = ZR_KEY_HOME;
      break;
    case (uint8_t)'F':
      key = ZR_KEY_END;
      break;
    case (uint8_t)'P':
      key = ZR_KEY_F1;
      break;
    case (uint8_t)'Q':
      key = ZR_KEY_F2;
      break;
    case (uint8_t)'R':
      key = ZR_KEY_F3;
      break;
    case (uint8_t)'S':
      key = ZR_KEY_F4;
      break;
    default:
      return false;
  }

  *out_key = key;
  *out_consumed = 3u;
  return true;
}

static uint32_t zr__mods_from_xterm_btn(uint32_t b) {
  uint32_t mods = 0u;
  if ((b & 4u) != 0u) {
    mods |= ZR_MOD_SHIFT;
  }
  if ((b & 8u) != 0u) {
    mods |= ZR_MOD_ALT;
  }
  if ((b & 16u) != 0u) {
    mods |= ZR_MOD_CTRL;
  }
  return mods;
}

static uint32_t zr__buttons_mask_from_base(uint32_t base) {
  if (base > 2u) {
    return 0u;
  }
  return 1u << base;
}

static bool zr__parse_sgr_mouse(const uint8_t* bytes, size_t len, size_t i, uint32_t time_ms, zr_event_queue_t* q,
                                size_t* out_consumed) {
  if (!bytes || !q || !out_consumed) {
    return false;
  }
  if ((i + 3u) >= len) {
    return false;
  }
  if (bytes[i] != ZR_IN_ESC || bytes[i + 1u] != (uint8_t)'[' || bytes[i + 2u] != (uint8_t)'<') {
    return false;
  }

  size_t j = i + 3u;
  uint32_t b = 0u;
  uint32_t x = 0u;
  uint32_t y = 0u;

  if (!zr__parse_u32_dec(bytes, len, &j, &b)) {
    return false;
  }
  if (j >= len || bytes[j] != (uint8_t)';') {
    return false;
  }
  j++;
  if (!zr__parse_u32_dec(bytes, len, &j, &x)) {
    return false;
  }
  if (j >= len || bytes[j] != (uint8_t)';') {
    return false;
  }
  j++;
  if (!zr__parse_u32_dec(bytes, len, &j, &y)) {
    return false;
  }
  if (j >= len) {
    return false;
  }

  const uint8_t term = bytes[j];
  if (term != (uint8_t)'M' && term != (uint8_t)'m') {
    return false;
  }

  const int32_t xi = (x > 0u && x <= (uint32_t)INT32_MAX) ? (int32_t)(x - 1u) : 0;
  const int32_t yi = (y > 0u && y <= (uint32_t)INT32_MAX) ? (int32_t)(y - 1u) : 0;
  const uint32_t mods = zr__mods_from_xterm_btn(b);

  const uint32_t base = b & 3u;
  const uint32_t is_motion = (b & 32u) != 0u ? 1u : 0u;
  const uint32_t is_wheel = (b & 64u) != 0u ? 1u : 0u;

  uint32_t kind = (uint32_t)ZR_MOUSE_MOVE;
  uint32_t buttons = 0u;
  int32_t wheel_x = 0;
  int32_t wheel_y = 0;

  if (is_wheel) {
    kind = (uint32_t)ZR_MOUSE_WHEEL;
    if (base == 0u) {
      wheel_y = 1;
    } else if (base == 1u) {
      wheel_y = -1;
    } else if (base == 2u) {
      wheel_x = 1;
    } else {
      wheel_x = -1;
    }
  } else if (is_motion) {
    if (base != 3u) {
      buttons = zr__buttons_mask_from_base(base);
    }
    kind = buttons != 0u ? (uint32_t)ZR_MOUSE_DRAG : (uint32_t)ZR_MOUSE_MOVE;
  } else if (term == (uint8_t)'m') {
    kind = (uint32_t)ZR_MOUSE_UP;
    buttons = zr__buttons_mask_from_base(base);
  } else if (base == 3u) {
    kind = (uint32_t)ZR_MOUSE_MOVE;
    buttons = 0u;
  } else {
    kind = (uint32_t)ZR_MOUSE_DOWN;
    buttons = zr__buttons_mask_from_base(base);
  }

  zr__push_mouse(q, time_ms, xi, yi, kind, mods, buttons, wheel_x, wheel_y);
  *out_consumed = (j + 1u) - i;
  return true;
}

static bool zr__esc_is_incomplete_supported(const uint8_t* bytes, size_t len, size_t i) {
  if (!bytes || i >= len || bytes[i] != ZR_IN_ESC) {
    return false;
  }
  if ((i + 1u) >= len) {
    return true;
  }

  const uint8_t b1 = bytes[i + 1u];
  if (b1 == (uint8_t)'[') {
    if ((i + 2u) >= len) {
      return true;
    }

    const uint8_t b2 = bytes[i + 2u];
    if (b2 == (uint8_t)'<') {
      for (size_t j = i + 3u; j < len; j++) {
        if (bytes[j] == (uint8_t)'M' || bytes[j] == (uint8_t)'m') {
          return false;
        }
      }
      return true;
    }

    for (size_t j = i + 2u; j < len; j++) {
      const uint8_t t = bytes[j];
      if (zr__is_digit(t) || t == (uint8_t)';') {
        continue;
      }
      return false;
    }
    return true;
  }

  if (b1 == (uint8_t)'O') {
    return (i + 2u) >= len;
  }

  return false;
}

static bool zr__utf8_is_incomplete_supported(const uint8_t* bytes, size_t len, size_t i) {
  if (!bytes || i >= len) {
    return false;
  }

  const uint8_t b0 = bytes[i];
  uint8_t need = 0u;
  if (b0 >= 0xC2u && b0 <= 0xDFu) {
    need = 2u;
  } else if (b0 >= 0xE0u && b0 <= 0xEFu) {
    need = 3u;
  } else if (b0 >= 0xF0u && b0 <= 0xF4u) {
    need = 4u;
  } else {
    return false;
  }

  return (len - i) < (size_t)need;
}

static size_t zr__consume_escape(zr_event_queue_t* q, const uint8_t* bytes, size_t len, size_t i, uint32_t time_ms) {
  if (!q || !bytes || i >= len || bytes[i] != ZR_IN_ESC) {
    return 0u;
  }

  zr_key_t key = ZR_KEY_UNKNOWN;
  uint32_t mods = 0u;
  size_t consumed = 0u;

  if ((i + 2u) < len && bytes[i + 1u] == (uint8_t)'[') {
    if (bytes[i + 2u] == (uint8_t)'<') {
      if (zr__parse_sgr_mouse(bytes, len, i, time_ms, q, &consumed)) {
        return consumed;
      }
    }
    if (zr__parse_csi_paste_marker(bytes, len, i, &consumed)) {
      return consumed;
    }
    if (zr__parse_csi_simple_key(bytes, len, i, &key, &mods, &consumed)) {
      zr__push_key(q, time_ms, key, mods);
      return consumed;
    }
    if (zr__parse_csi_tilde_key(bytes, len, i, &key, &mods, &consumed)) {
      zr__push_key(q, time_ms, key, mods);
      return consumed;
    }
  }

  if (zr__parse_ss3_key(bytes, len, i, &key, &consumed)) {
    zr__push_key(q, time_ms, key, 0u);
    return consumed;
  }

  zr__push_key(q, time_ms, ZR_KEY_ESCAPE, 0u);
  return 1u;
}

static size_t zr_input_parse_bytes_internal(zr_event_queue_t* q, const uint8_t* bytes, size_t len, uint32_t time_ms,
                                            bool stop_before_incomplete) {
  if (!q || (!bytes && len != 0u)) {
    return 0u;
  }

  size_t i = 0u;
  while (i < len) {
    const uint8_t b = bytes[i];

    if (b == ZR_IN_ESC) {
      if (stop_before_incomplete && zr__esc_is_incomplete_supported(bytes, len, i)) {
        break;
      }
      i += zr__consume_escape(q, bytes, len, i, time_ms);
      continue;
    }

    if (b == (uint8_t)'\r' || b == (uint8_t)'\n') {
      zr__push_key(q, time_ms, ZR_KEY_ENTER, 0u);
      i += 1u;
      continue;
    }
    if (b == (uint8_t)'\t') {
      zr__push_key(q, time_ms, ZR_KEY_TAB, 0u);
      i += 1u;
      continue;
    }
    if (b == ZR_IN_DEL) {
      zr__push_key(q, time_ms, ZR_KEY_BACKSPACE, 0u);
      i += 1u;
      continue;
    }

    if (b < 0x20u) {
      i += 1u;
      continue;
    }

    if (stop_before_incomplete && zr__utf8_is_incomplete_supported(bytes, len, i)) {
      break;
    }

    zr_utf8_decode_result_t d = zr_utf8_decode_one(bytes + i, len - i);
    if (d.size == 0u) {
      i += 1u;
      continue;
    }
    zr__push_text_scalar(q, time_ms, d.scalar);
    i += (size_t)d.size;
  }

  return i;
}

void zr_input_parse_bytes(zr_event_queue_t* q, const uint8_t* bytes, size_t len, uint32_t time_ms) {
  (void)zr_input_parse_bytes_internal(q, bytes, len, time_ms, false);
}

size_t zr_input_parse_bytes_prefix(zr_event_queue_t* q, const uint8_t* bytes, size_t len, uint32_t time_ms) {
  return zr_input_parse_bytes_internal(q, bytes, len, time_ms, true);
}

