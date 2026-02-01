/*
  tests/unit/test_event_queue.c — Unit tests for event queue coalescing/drop policy.
*/

#include "zr_test.h"

#include "core/zr_event_queue.h"

#include <string.h>

static zr_event_t zr_make_key(uint32_t time_ms, zr_key_t key) {
  zr_event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ZR_EV_KEY;
  ev.time_ms = time_ms;
  ev.flags = 0u;
  ev.u.key.key = (uint32_t)key;
  ev.u.key.action = (uint32_t)ZR_KEY_ACTION_DOWN;
  return ev;
}

static zr_event_t zr_make_resize(uint32_t cols, uint32_t rows) {
  zr_event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ZR_EV_RESIZE;
  ev.time_ms = 0u;
  ev.flags = 0u;
  ev.u.resize.cols = cols;
  ev.u.resize.rows = rows;
  return ev;
}

static zr_event_t zr_make_mouse_move(int32_t x, int32_t y) {
  zr_event_t ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ZR_EV_MOUSE;
  ev.time_ms = 0u;
  ev.flags = 0u;
  ev.u.mouse.x = x;
  ev.u.mouse.y = y;
  ev.u.mouse.kind = (uint32_t)ZR_MOUSE_MOVE;
  return ev;
}

ZR_TEST_UNIT(event_queue_coalesces_resize_last_wins) {
  zr_event_t storage[8];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 8u, NULL, 0u), ZR_OK);

  zr_event_t ev = zr_make_resize(80u, 24u);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  ev = zr_make_resize(120u, 40u);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_RESIZE);
  ZR_ASSERT_EQ_U32(head.u.resize.cols, 120u);
  ZR_ASSERT_EQ_U32(head.u.resize.rows, 40u);
}

ZR_TEST_UNIT(event_queue_coalesces_mouse_move_last_wins) {
  zr_event_t storage[8];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 8u, NULL, 0u), ZR_OK);

  zr_event_t ev = zr_make_mouse_move(1, 2);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  ev = zr_make_mouse_move(9, 10);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_MOUSE);
  ZR_ASSERT_EQ_U32((uint32_t)head.u.mouse.x, (uint32_t)9u);
  ZR_ASSERT_EQ_U32((uint32_t)head.u.mouse.y, (uint32_t)10u);
}

ZR_TEST_UNIT(event_queue_drops_oldest_when_full) {
  zr_event_t storage[3];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 3u, NULL, 0u), ZR_OK);

  zr_event_t ev = zr_make_key(1u, ZR_KEY_ENTER);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ev = zr_make_key(2u, ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ev = zr_make_key(3u, ZR_KEY_ESCAPE);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 3u);

  /* Full: pushing another non-coalescible event drops the oldest. */
  ev = zr_make_key(4u, ZR_KEY_BACKSPACE);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 3u);
  ZR_ASSERT_EQ_U32(q.dropped_due_to_full, 1u);

  zr_event_t out;
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &out));
  ZR_ASSERT_EQ_U32(out.u.key.key, (uint32_t)ZR_KEY_TAB);
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &out));
  ZR_ASSERT_EQ_U32(out.u.key.key, (uint32_t)ZR_KEY_ESCAPE);
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &out));
  ZR_ASSERT_EQ_U32(out.u.key.key, (uint32_t)ZR_KEY_BACKSPACE);
}

ZR_TEST_UNIT(event_queue_full_resize_still_coalesces) {
  zr_event_t storage[3];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 3u, NULL, 0u), ZR_OK);

  zr_event_t ev = zr_make_resize(10u, 10u);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ev = zr_make_key(0u, ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ev = zr_make_key(0u, ZR_KEY_ENTER);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 3u);

  /* Queue is full; resize replaces existing resize (no drop). */
  ev = zr_make_resize(99u, 77u);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(q.dropped_due_to_full, 0u);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 3u);

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_RESIZE);
  ZR_ASSERT_EQ_U32(head.u.resize.cols, 99u);
  ZR_ASSERT_EQ_U32(head.u.resize.rows, 77u);
}

