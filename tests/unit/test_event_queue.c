/*
  tests/unit/test_event_queue.c — Unit tests for event queue coalescing/drop policy.

  Why: Validates the engine's deterministic event coalescing policy (resize and
  mouse-move events use "last wins" semantics) and the "drop oldest" behavior
  when the queue is full with non-coalescible events.

  Scenarios tested:
    - Consecutive resize events coalesce (last wins, queue size = 1)
    - Consecutive mouse-move events coalesce (last wins, queue size = 1)
    - Non-coalescible events: when full, oldest event is dropped
    - Coalescible events replace in-place even when queue is full
*/

#include "zr_test.h"

#include "core/zr_event_queue.h"

#include <string.h>

/* --- Test Helpers --- */

/* Create a key-down event with given timestamp and key code. */
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

/* Create a resize event with given dimensions. */
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

/* Create a mouse-move event at given position. */
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

/*
 * Test: event_queue_coalesces_resize_last_wins
 *
 * Scenario: Consecutive resize events are coalesced using "last wins"
 *           semantics — only the most recent dimensions are retained.
 *
 * Arrange: Initialize 8-event queue.
 * Act:     Push resize 80x24, then resize 120x40.
 * Assert:  Queue count remains 1; peeked event has 120x40 dimensions.
 */
ZR_TEST_UNIT(event_queue_coalesces_resize_last_wins) {
  /* --- Arrange --- */
  zr_event_t storage[8];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 8u, NULL, 0u), ZR_OK);

  /* --- Act: Push two resize events --- */
  zr_event_t ev = zr_make_resize(80u, 24u);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  ev = zr_make_resize(120u, 40u);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);

  /* --- Assert: Coalesced to single event with last dimensions --- */
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_RESIZE);
  ZR_ASSERT_EQ_U32(head.u.resize.cols, 120u);
  ZR_ASSERT_EQ_U32(head.u.resize.rows, 40u);
}

/*
 * Test: event_queue_coalesces_mouse_move_last_wins
 *
 * Scenario: Consecutive mouse-move events are coalesced using "last wins"
 *           semantics — only the most recent position is retained.
 *
 * Arrange: Initialize 8-event queue.
 * Act:     Push mouse-move at (1,2), then at (9,10).
 * Assert:  Queue count remains 1; peeked event has position (9,10).
 */
ZR_TEST_UNIT(event_queue_coalesces_mouse_move_last_wins) {
  /* --- Arrange --- */
  zr_event_t storage[8];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 8u, NULL, 0u), ZR_OK);

  /* --- Act: Push two mouse-move events --- */
  zr_event_t ev = zr_make_mouse_move(1, 2);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  ev = zr_make_mouse_move(9, 10);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);

  /* --- Assert: Coalesced to single event with last position --- */
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_MOUSE);
  ZR_ASSERT_EQ_U32((uint32_t)head.u.mouse.x, (uint32_t)9u);
  ZR_ASSERT_EQ_U32((uint32_t)head.u.mouse.y, (uint32_t)10u);
}

/*
 * Test: event_queue_drops_oldest_when_full
 *
 * Scenario: When the queue is full and a non-coalescible event is pushed,
 *           the oldest event is dropped to make room.
 *
 * Arrange: Initialize 3-event queue.
 * Act:     Push 3 key events (fills queue), then push a 4th.
 * Assert:  4th push succeeds; dropped_due_to_full = 1; oldest (ENTER) is gone;
 *          remaining events are TAB, ESCAPE, BACKSPACE.
 */
ZR_TEST_UNIT(event_queue_drops_oldest_when_full) {
  /* --- Arrange --- */
  zr_event_t storage[3];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 3u, NULL, 0u), ZR_OK);

  /* --- Act: Fill queue with key events --- */
  zr_event_t ev = zr_make_key(1u, ZR_KEY_ENTER);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ev = zr_make_key(2u, ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ev = zr_make_key(3u, ZR_KEY_ESCAPE);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 3u);

  /* --- Act: Push when full (non-coalescible) --- */
  ev = zr_make_key(4u, ZR_KEY_BACKSPACE);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);

  /* --- Assert: Oldest dropped, drop counter incremented --- */
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 3u);
  ZR_ASSERT_EQ_U32(q.dropped_due_to_full, 1u);

  /* --- Assert: Remaining events in order (oldest dropped) --- */
  zr_event_t out;
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &out));
  ZR_ASSERT_EQ_U32(out.u.key.key, (uint32_t)ZR_KEY_TAB);
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &out));
  ZR_ASSERT_EQ_U32(out.u.key.key, (uint32_t)ZR_KEY_ESCAPE);
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &out));
  ZR_ASSERT_EQ_U32(out.u.key.key, (uint32_t)ZR_KEY_BACKSPACE);
}

/*
 * Test: event_queue_full_resize_still_coalesces
 *
 * Scenario: Even when the queue is full, a resize event coalesces with an
 *           existing resize (replaces in-place) without dropping any events.
 *
 * Arrange: Initialize 3-event queue; push resize + 2 key events (fills queue).
 * Act:     Push another resize event.
 * Assert:  No events dropped (dropped_due_to_full = 0); count still 3;
 *          resize event updated to new dimensions.
 */
ZR_TEST_UNIT(event_queue_full_resize_still_coalesces) {
  /* --- Arrange --- */
  zr_event_t storage[3];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 3u, NULL, 0u), ZR_OK);

  /* Fill queue: resize + 2 keys */
  zr_event_t ev = zr_make_resize(10u, 10u);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ev = zr_make_key(0u, ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ev = zr_make_key(0u, ZR_KEY_ENTER);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 3u);

  /* --- Act: Push coalescible resize when full --- */
  ev = zr_make_resize(99u, 77u);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);

  /* --- Assert: Coalesced in-place, no drop --- */
  ZR_ASSERT_EQ_U32(q.dropped_due_to_full, 0u);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 3u);

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_RESIZE);
  ZR_ASSERT_EQ_U32(head.u.resize.cols, 99u);
  ZR_ASSERT_EQ_U32(head.u.resize.rows, 77u);
}

ZR_TEST_UNIT(event_queue_post_paste_rejects_payload_too_large) {
  zr_event_t storage[4];
  uint8_t user_bytes[8];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 4u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  const uint8_t bytes[9] = {0u};
  ZR_ASSERT_EQ_U32(zr_event_queue_post_paste(&q, 1u, bytes, (uint32_t)sizeof(bytes)), ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 0u);
  ZR_ASSERT_EQ_U32(q.user_used, 0u);
}

ZR_TEST_UNIT(event_queue_post_paste_drops_oldest_and_frees_user_payload) {
  zr_event_t storage[1];
  uint8_t user_bytes[8];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 1u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  const uint8_t user_payload[] = {(uint8_t)'u', (uint8_t)'s', (uint8_t)'e', (uint8_t)'r'};
  const uint8_t paste_payload[] = {(uint8_t)'p', (uint8_t)'a', (uint8_t)'s', (uint8_t)'t'};

  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 1u, 42u, user_payload, (uint32_t)sizeof(user_payload)), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);
  ZR_ASSERT_EQ_U32(q.user_used, (uint32_t)sizeof(user_payload));

  ZR_ASSERT_EQ_U32(zr_event_queue_post_paste(&q, 2u, paste_payload, (uint32_t)sizeof(paste_payload)), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);
  ZR_ASSERT_EQ_U32(q.user_used, (uint32_t)sizeof(paste_payload));

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_PASTE);

  const uint8_t* bytes = NULL;
  uint32_t len = 0u;
  ZR_ASSERT_TRUE(zr_event_queue_paste_payload_view(&q, &head, &bytes, &len));
  ZR_ASSERT_EQ_U32(len, (uint32_t)sizeof(paste_payload));
  ZR_ASSERT_TRUE(memcmp(bytes, paste_payload, sizeof(paste_payload)) == 0);

  zr_event_t out;
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &out));
  ZR_ASSERT_EQ_U32(out.type, (uint32_t)ZR_EV_PASTE);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 0u);
  ZR_ASSERT_EQ_U32(q.user_used, 0u);
}

ZR_TEST_UNIT(event_queue_push_drops_paste_and_frees_payload_bytes) {
  zr_event_t storage[1];
  uint8_t user_bytes[8];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 1u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  const uint8_t paste_payload[] = {(uint8_t)'p', (uint8_t)'a', (uint8_t)'s', (uint8_t)'t'};
  ZR_ASSERT_EQ_U32(zr_event_queue_post_paste(&q, 1u, paste_payload, (uint32_t)sizeof(paste_payload)), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);
  ZR_ASSERT_EQ_U32(q.user_used, (uint32_t)sizeof(paste_payload));

  zr_event_t ev = zr_make_key(2u, ZR_KEY_ESCAPE);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);
  ZR_ASSERT_EQ_U32(q.dropped_due_to_full, 1u);
  ZR_ASSERT_EQ_U32(q.user_used, 0u);

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_KEY);
  ZR_ASSERT_EQ_U32(head.u.key.key, (uint32_t)ZR_KEY_ESCAPE);
}

ZR_TEST_UNIT(event_queue_post_paste_does_not_drop_when_ring_full) {
  zr_event_t storage[2];
  uint8_t user_bytes[8];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 2u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  zr_event_t ev = zr_make_key(1u, ZR_KEY_ESCAPE);
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &ev), ZR_OK);

  /* Fill the payload ring completely with a USER event. */
  uint8_t full_payload[8];
  memset(full_payload, 0xA5, sizeof(full_payload));
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 2u, 123u, full_payload, (uint32_t)sizeof(full_payload)), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 2u);
  ZR_ASSERT_EQ_U32(q.user_used, (uint32_t)sizeof(full_payload));

  const uint8_t one_byte[] = {0x5Au};
  ZR_ASSERT_EQ_U32(zr_event_queue_post_paste(&q, 3u, one_byte, (uint32_t)sizeof(one_byte)), ZR_ERR_LIMIT);

  /* Must not drop the head event when the paste cannot be enqueued. */
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 2u);
  ZR_ASSERT_EQ_U32(q.dropped_due_to_full, 0u);
  ZR_ASSERT_EQ_U32(q.user_used, (uint32_t)sizeof(full_payload));

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_KEY);
}
