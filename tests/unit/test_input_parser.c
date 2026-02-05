/*
  tests/unit/test_input_parser.c — Unit tests for VT input parsing.

  Why: Validates deterministic parsing for UTF-8 text, key sequences, SGR
  mouse reports, paste markers, and prefix/incomplete handling.
*/

#include "zr_test.h"

#include "core/zr_input_parser.h"

#include <string.h>

static void zr_init_queue(zr_test_ctx_t* ctx, zr_event_queue_t* out_q, zr_event_t* storage, uint32_t cap) {
  ZR_ASSERT_TRUE(out_q != NULL);
  memset(out_q, 0, sizeof(*out_q));
  ZR_ASSERT_EQ_U32(zr_event_queue_init(out_q, storage, cap, NULL, 0u), ZR_OK);
}

ZR_TEST_UNIT(input_parser_decodes_utf8_and_replacement) {
  zr_event_t storage[16];
  zr_event_queue_t q;
  zr_init_queue(ctx, &q, storage, 16u);

  /* "A", EURO SIGN, and an invalid continuation byte -> U+FFFD. */
  const uint8_t in[] = {(uint8_t)'A', 0xE2u, 0x82u, 0xACu, 0x80u};
  zr_input_parse_bytes(&q, in, sizeof(in), 123u);

  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 3u);

  zr_event_t ev;
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_TEXT);
  ZR_ASSERT_EQ_U32(ev.u.text.codepoint, (uint32_t)'A');

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_TEXT);
  ZR_ASSERT_EQ_U32(ev.u.text.codepoint, 0x20ACu);

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_TEXT);
  ZR_ASSERT_EQ_U32(ev.u.text.codepoint, 0xFFFDu);
}

ZR_TEST_UNIT(input_parser_parses_keys_mods_and_ss3) {
  zr_event_t storage[32];
  zr_event_queue_t q;
  zr_init_queue(ctx, &q, storage, 32u);

  const uint8_t in[] = {
      0x1Bu, (uint8_t)'[', (uint8_t)'A', /* Up */
      0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'5', (uint8_t)'D', /* Ctrl+Left */
      0x1Bu, (uint8_t)'O', (uint8_t)'P', /* F1 */
      0x1Bu, (uint8_t)'[', (uint8_t)'2', (uint8_t)'4', (uint8_t)'~', /* F12 */
      (uint8_t)'\r',
      (uint8_t)'\t',
      0x7Fu,
  };
  zr_input_parse_bytes(&q, in, sizeof(in), 7u);

  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 7u);

  zr_event_t ev;
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_KEY);
  ZR_ASSERT_EQ_U32(ev.u.key.key, (uint32_t)ZR_KEY_UP);
  ZR_ASSERT_EQ_U32(ev.u.key.mods, 0u);

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_KEY);
  ZR_ASSERT_EQ_U32(ev.u.key.key, (uint32_t)ZR_KEY_LEFT);
  ZR_ASSERT_EQ_U32(ev.u.key.mods, ZR_MOD_CTRL);

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_KEY);
  ZR_ASSERT_EQ_U32(ev.u.key.key, (uint32_t)ZR_KEY_F1);

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_KEY);
  ZR_ASSERT_EQ_U32(ev.u.key.key, (uint32_t)ZR_KEY_F12);

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_KEY);
  ZR_ASSERT_EQ_U32(ev.u.key.key, (uint32_t)ZR_KEY_ENTER);

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_KEY);
  ZR_ASSERT_EQ_U32(ev.u.key.key, (uint32_t)ZR_KEY_TAB);

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_KEY);
  ZR_ASSERT_EQ_U32(ev.u.key.key, (uint32_t)ZR_KEY_BACKSPACE);
}

ZR_TEST_UNIT(input_parser_parses_sgr_mouse) {
  zr_event_t storage[16];
  zr_event_queue_t q;
  zr_init_queue(ctx, &q, storage, 16u);

  const uint8_t in[] = {
      0x1Bu, (uint8_t)'[', (uint8_t)'<', (uint8_t)'0', (uint8_t)';', (uint8_t)'1', (uint8_t)'0', (uint8_t)';',
      (uint8_t)'5', (uint8_t)'M', /* down left at (9,4) */

      0x1Bu, (uint8_t)'[', (uint8_t)'<', (uint8_t)'3', (uint8_t)'5', (uint8_t)';', (uint8_t)'1', (uint8_t)'0',
      (uint8_t)';', (uint8_t)'5', (uint8_t)'M', /* move at (9,4) */

      0x1Bu, (uint8_t)'[', (uint8_t)'<', (uint8_t)'6', (uint8_t)'4', (uint8_t)';', (uint8_t)'1', (uint8_t)'0',
      (uint8_t)';', (uint8_t)'5', (uint8_t)'M', /* wheel up at (9,4) */
  };
  zr_input_parse_bytes(&q, in, sizeof(in), 17u);

  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 3u);

  zr_event_t ev;
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_MOUSE);
  ZR_ASSERT_EQ_U32(ev.u.mouse.kind, (uint32_t)ZR_MOUSE_DOWN);
  ZR_ASSERT_EQ_U32((uint32_t)ev.u.mouse.x, 9u);
  ZR_ASSERT_EQ_U32((uint32_t)ev.u.mouse.y, 4u);
  ZR_ASSERT_EQ_U32(ev.u.mouse.buttons, 1u);

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_MOUSE);
  ZR_ASSERT_EQ_U32(ev.u.mouse.kind, (uint32_t)ZR_MOUSE_MOVE);
  ZR_ASSERT_EQ_U32(ev.u.mouse.buttons, 0u);

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_MOUSE);
  ZR_ASSERT_EQ_U32(ev.u.mouse.kind, (uint32_t)ZR_MOUSE_WHEEL);
  ZR_ASSERT_EQ_U32((uint32_t)ev.u.mouse.wheel_y, 1u);
}

ZR_TEST_UNIT(input_parser_prefix_stops_on_incomplete_supported_sequences) {
  zr_event_t storage[16];
  zr_event_queue_t q;
  zr_init_queue(ctx, &q, storage, 16u);

  const uint8_t esc_incomplete[] = {0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'5'};
  const size_t esc_consumed = zr_input_parse_bytes_prefix(&q, esc_incomplete, sizeof(esc_incomplete), 0u);
  ZR_ASSERT_EQ_U32((uint32_t)esc_consumed, 0u);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 0u);

  const uint8_t utf8_incomplete[] = {0xE2u, 0x82u};
  const size_t utf8_consumed = zr_input_parse_bytes_prefix(&q, utf8_incomplete, sizeof(utf8_incomplete), 0u);
  ZR_ASSERT_EQ_U32((uint32_t)utf8_consumed, 0u);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 0u);

  const uint8_t full_key[] = {0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'5', (uint8_t)'A'};
  ZR_ASSERT_EQ_U32((uint32_t)zr_input_parse_bytes_prefix(&q, full_key, sizeof(full_key), 0u), (uint32_t)sizeof(full_key));
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  zr_event_t ev;
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_KEY);
  ZR_ASSERT_EQ_U32(ev.u.key.key, (uint32_t)ZR_KEY_UP);
  ZR_ASSERT_EQ_U32(ev.u.key.mods, ZR_MOD_CTRL);

  const uint8_t full_utf8[] = {0xE2u, 0x82u, 0xACu};
  ZR_ASSERT_EQ_U32((uint32_t)zr_input_parse_bytes_prefix(&q, full_utf8, sizeof(full_utf8), 0u),
                   (uint32_t)sizeof(full_utf8));
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_TEXT);
  ZR_ASSERT_EQ_U32(ev.u.text.codepoint, 0x20ACu);
}

ZR_TEST_UNIT(input_parser_ignores_bracketed_paste_markers) {
  zr_event_t storage[16];
  zr_event_queue_t q;
  zr_init_queue(ctx, &q, storage, 16u);

  const uint8_t in[] = {
      0x1Bu, (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'0', (uint8_t)'~',
      (uint8_t)'X',
      0x1Bu, (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'1', (uint8_t)'~',
  };
  zr_input_parse_bytes(&q, in, sizeof(in), 3u);

  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);
  zr_event_t ev;
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev));
  ZR_ASSERT_EQ_U32(ev.type, (uint32_t)ZR_EV_TEXT);
  ZR_ASSERT_EQ_U32(ev.u.text.codepoint, (uint32_t)'X');
}
