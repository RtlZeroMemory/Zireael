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

/* Parse terminal input bytes into key/text events; handles basic CSI arrow keys and controls. */
void zr_input_parse_bytes(zr_event_queue_t* q, const uint8_t* bytes, size_t len, uint32_t time_ms) {
  if (!q || (!bytes && len != 0u)) {
    return;
  }

  size_t i = 0u;
  while (i < len) {
    const uint8_t b = bytes[i];

    /* Minimal CSI arrow keys: ESC [ A/B/C/D */
    if (b == 0x1Bu) {
      if ((i + 2u) < len && bytes[i + 1u] == (uint8_t)'[') {
        const uint8_t c = bytes[i + 2u];
        if (c == (uint8_t)'A') {
          zr__push_key(q, time_ms, ZR_KEY_UP);
          i += 3u;
          continue;
        }
        if (c == (uint8_t)'B') {
          zr__push_key(q, time_ms, ZR_KEY_DOWN);
          i += 3u;
          continue;
        }
        if (c == (uint8_t)'C') {
          zr__push_key(q, time_ms, ZR_KEY_RIGHT);
          i += 3u;
          continue;
        }
        if (c == (uint8_t)'D') {
          zr__push_key(q, time_ms, ZR_KEY_LEFT);
          i += 3u;
          continue;
        }
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

