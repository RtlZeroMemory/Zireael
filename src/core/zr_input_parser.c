/*
  src/core/zr_input_parser.c — Minimal input byte parser implementation.

  Why: Provides a deterministic, bounds-checked parser for platform input bytes
  suitable for fuzzing. Unknown escape sequences are handled without hangs.
*/

#include "core/zr_input_parser.h"

#include <string.h>

static void zr__push_key(zr_event_queue_t* q, uint32_t time_ms, zr_key_t key) {
  zr_event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ZR_EV_KEY;
  ev.time_ms = time_ms;
  ev.flags = 0u;
  ev.u.key.key = (uint32_t)key;
  ev.u.key.mods = 0u;
  ev.u.key.action = (uint32_t)ZR_KEY_ACTION_DOWN;
  ev.u.key.reserved0 = 0u;
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

static void zr__push_text_byte(zr_event_queue_t* q, uint32_t time_ms, uint8_t b) {
  zr_event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ZR_EV_TEXT;
  ev.time_ms = time_ms;
  ev.flags = 0u;
  ev.u.text.codepoint = (uint32_t)b;
  ev.u.text.reserved0 = 0u;
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
    v = (v * 10u) + d;
    i++;
  }

  *io_i = i;
  *out_val = v;
  return true;
}

static bool zr__parse_csi_tilde_key(const uint8_t* bytes, size_t len, size_t i, zr_key_t* out_key,
                                   size_t* out_consumed) {
  if (!bytes || !out_key || !out_consumed) {
    return false;
  }
  if ((i + 2u) >= len) {
    return false;
  }
  if (bytes[i] != 0x1Bu || bytes[i + 1u] != (uint8_t)'[') {
    return false;
  }

  size_t j = i + 2u;
  uint32_t first = 0u;
  if (!zr__parse_u32_dec(bytes, len, &j, &first)) {
    return false;
  }

  while (j < len && bytes[j] != (uint8_t)'~') {
    /* Skip modifier parameters (e.g. "1;5~"). */
    if (bytes[j] == (uint8_t)';') {
      j++;
      uint32_t dummy = 0u;
      if (!zr__parse_u32_dec(bytes, len, &j, &dummy)) {
        return false;
      }
      continue;
    }
    return false;
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
    default:
      return false;
  }

  *out_key = key;
  *out_consumed = (j + 1u) - i;
  return true;
}

static bool zr__parse_csi_simple_key(const uint8_t* bytes, size_t len, size_t i, zr_key_t* out_key,
                                    size_t* out_consumed) {
  if (!bytes || !out_key || !out_consumed) {
    return false;
  }
  if ((i + 2u) >= len) {
    return false;
  }
  if (bytes[i] != 0x1Bu || bytes[i + 1u] != (uint8_t)'[') {
    return false;
  }

  /*
    CSI sequences we accept here:
      - ESC [ A/B/C/D (arrows)
      - ESC [ <params> A/B/C/D (arrows with modifiers)
      - ESC [ H/F (home/end) and their param forms
  */
  size_t j = i + 2u;
  while (j < len) {
    const uint8_t b = bytes[j];
    if (zr__is_digit(b) || b == (uint8_t)';') {
      j++;
      continue;
    }
    break;
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
  if (bytes[i] != 0x1Bu || bytes[i + 1u] != (uint8_t)'O') {
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
  if (bytes[i] != 0x1Bu || bytes[i + 1u] != (uint8_t)'[' || bytes[i + 2u] != (uint8_t)'<') {
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

  /* Term coordinates are 1-based; clamp to 0-based int32. */
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
    /*
      SGR wheel encoding:
        64 = wheel up, 65 = wheel down, 66 = wheel right, 67 = wheel left.
    */
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
    /*
      In any-event mouse tracking (DECSET 1003), motion with no buttons pressed
      is encoded as base=3 with the motion bit set (e.g. b=35). Treat that as a
      MOVE, not as a button release.
    */
    if (base != 3u) {
      buttons = zr__buttons_mask_from_base(base);
    }
    kind = buttons != 0u ? (uint32_t)ZR_MOUSE_DRAG : (uint32_t)ZR_MOUSE_MOVE;
  } else if (term == (uint8_t)'m') {
    kind = (uint32_t)ZR_MOUSE_UP;
    buttons = zr__buttons_mask_from_base(base);
  } else if (base == 3u) {
    /* No-buttons report (hover/move). Avoid spurious UP events. */
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

/*
  Parse terminal input bytes into key/mouse/text events.

  Why: The engine reads raw bytes on POSIX backends. This parser must accept
  common VT/xterm control sequences (arrows, home/end, SGR mouse) and degrade
  deterministically on unknown sequences without hangs.
*/
void zr_input_parse_bytes(zr_event_queue_t* q, const uint8_t* bytes, size_t len, uint32_t time_ms) {
  if (!q || (!bytes && len != 0u)) {
    return;
  }

  size_t i = 0u;
  while (i < len) {
    const uint8_t b = bytes[i];

    /* --- Escape-driven VT sequences --- */
    if (b == 0x1Bu) {
      zr_key_t key = ZR_KEY_UNKNOWN;
      size_t consumed = 0u;

      if ((i + 2u) < len && bytes[i + 1u] == (uint8_t)'[') {
        /* SGR mouse: ESC [ < ... (M or m) */
        if ((i + 2u) < len && bytes[i + 2u] == (uint8_t)'<') {
          if (zr__parse_sgr_mouse(bytes, len, i, time_ms, q, &consumed)) {
            i += consumed;
            continue;
          }
        }

        if (zr__parse_csi_simple_key(bytes, len, i, &key, &consumed)) {
          zr__push_key(q, time_ms, key);
          i += consumed;
          continue;
        }
        if (zr__parse_csi_tilde_key(bytes, len, i, &key, &consumed)) {
          zr__push_key(q, time_ms, key);
          i += consumed;
          continue;
        }
      }

      if (zr__parse_ss3_key(bytes, len, i, &key, &consumed)) {
        zr__push_key(q, time_ms, key);
        i += consumed;
        continue;
      }

      /* Deterministic fallback: treat bare ESC as an Escape key. */
      zr__push_key(q, time_ms, ZR_KEY_ESCAPE);
      i += 1u;
      continue;
    }

    if (b == (uint8_t)'\r' || b == (uint8_t)'\n') {
      zr__push_key(q, time_ms, ZR_KEY_ENTER);
      i += 1u;
      continue;
    }
    if (b == (uint8_t)'\t') {
      zr__push_key(q, time_ms, ZR_KEY_TAB);
      i += 1u;
      continue;
    }
    if (b == 0x7Fu) {
      zr__push_key(q, time_ms, ZR_KEY_BACKSPACE);
      i += 1u;
      continue;
    }

    /* MVP: treat all remaining bytes as text bytes (no UTF-8 decoding yet). */
    zr__push_text_byte(q, time_ms, b);
    i += 1u;
  }
}
