/*
  tests/unit/test_user_event_injection.c — Unit tests for thread-safe user event injection.

  Why: Validates the user event injection API which allows callers to post
  custom events with payloads. Tests ensure payload copying (not just pointer
  capture), capacity enforcement, and rejection when the queue is full.

  Scenarios tested:
    - Payload bytes are copied (caller buffer mutation doesn't affect queue)
    - Payload exceeding user_bytes capacity is rejected without partial enqueue
    - Post fails when event queue is full (preserves existing events)
*/

#include "zr_test.h"

#include "core/zr_event_queue.h"

#include <string.h>

/*
 * Test: user_event_injection_copies_payload_bytes
 *
 * Scenario: The event queue copies payload bytes on post_user; mutations
 *           to the caller's buffer do not affect the queued event.
 *
 * Arrange: Initialize queue with 4 events and 16-byte user payload buffer.
 * Act:     Post user event with 3-byte payload, then mutate caller's buffer.
 * Assert:  Peeked event payload contains original bytes, not mutated values.
 */
ZR_TEST_UNIT(user_event_injection_copies_payload_bytes) {
  /* --- Arrange --- */
  zr_event_t storage[4];
  uint8_t user_bytes[16];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 4u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  uint8_t payload[3] = {1u, 2u, 3u};

  /* --- Act: Post event, then mutate caller buffer --- */
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 7u, 0xBEEFu, payload, (uint32_t)sizeof(payload)), ZR_OK);

  payload[0] = 9u;
  payload[1] = 9u;
  payload[2] = 9u;

  /* --- Assert: Queued event has original payload bytes --- */
  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_USER);
  ZR_ASSERT_EQ_U32(head.u.user.hdr.tag, 0xBEEFu);
  ZR_ASSERT_EQ_U32(head.u.user.hdr.byte_len, 3u);

  const uint8_t* view = NULL;
  uint32_t view_len = 0u;
  ZR_ASSERT_TRUE(zr_event_queue_user_payload_view(&q, &head, &view, &view_len));
  ZR_ASSERT_EQ_U32(view_len, 3u);

  const uint8_t expected[3] = {1u, 2u, 3u};
  ZR_ASSERT_MEMEQ(view, expected, 3u);
}

/*
 * Test: user_event_injection_enforces_payload_cap_without_partial_enqueue
 *
 * Scenario: When the user payload buffer is full, subsequent post_user calls
 *           fail without partial enqueue (no event added, no bytes consumed).
 *
 * Arrange: Initialize queue with 4-byte user payload buffer.
 * Act:     Post 4-byte payload (fills buffer), attempt 1-byte payload.
 * Assert:  First post succeeds; second returns ZR_ERR_LIMIT; count unchanged.
 */
ZR_TEST_UNIT(user_event_injection_enforces_payload_cap_without_partial_enqueue) {
  /* --- Arrange --- */
  zr_event_t storage[4];
  uint8_t user_bytes[4];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 4u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  /* --- Act: Fill payload buffer --- */
  const uint8_t payload4[4] = {1u, 2u, 3u, 4u};
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 0u, 1u, payload4, 4u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  /* --- Act & Assert: Payload buffer full, rejects new post --- */
  const uint8_t payload1[1] = {9u};
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 0u, 2u, payload1, 1u), ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);
}

/*
 * Test: user_event_injection_rejects_when_queue_full
 *
 * Scenario: User event post fails when the event queue (not just payload
 *           buffer) is full, preserving existing events.
 *
 * Arrange: Initialize 1-event queue with key event already queued.
 * Act:     Attempt to post user event.
 * Assert:  Returns ZR_ERR_LIMIT; count unchanged; original key event preserved.
 */
ZR_TEST_UNIT(user_event_injection_rejects_when_queue_full) {
  /* --- Arrange --- */
  zr_event_t storage[1];
  uint8_t user_bytes[8];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 1u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  /* Fill queue with key event */
  zr_event_t key;
  memset(&key, 0, sizeof(key));
  key.type = ZR_EV_KEY;
  key.u.key.key = (uint32_t)ZR_KEY_TAB;
  key.u.key.action = (uint32_t)ZR_KEY_ACTION_DOWN;
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &key), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  /* --- Act: Attempt user post when queue is full --- */
  const uint8_t payload[1] = {0u};
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 0u, 1u, payload, 1u), ZR_ERR_LIMIT);

  /* --- Assert: Queue unchanged, original event preserved --- */
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_KEY);
}

/*
 * Test: user_event_injection_wrap_tracks_pad_and_avoids_corruption
 *
 * Scenario: Variable-sized payload allocation can require wrapping from the end
 *           of the user-bytes ring back to 0. When this happens, any remaining
 *           bytes at the end must be treated as pad until the read head wraps,
 *           otherwise a subsequent allocation could overwrite an older payload.
 *
 * Arrange: Small 10-byte user ring. Post payloads [3] and [6], pop the first
 *          (head becomes 3, tail near end). Post a third payload [3] which must
 *          wrap; then try to post another payload [1].
 * Assert:  The final [1] post is rejected (pad makes the ring effectively full),
 *          and the older [6] payload remains intact.
 */
ZR_TEST_UNIT(user_event_injection_wrap_tracks_pad_and_avoids_corruption) {
  zr_event_t storage[8];
  uint8_t user_bytes[10];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 8u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  const uint8_t payload_a[3] = {0xA1u, 0xA2u, 0xA3u};
  const uint8_t payload_b[6] = {0xB1u, 0xB2u, 0xB3u, 0xB4u, 0xB5u, 0xB6u};
  const uint8_t payload_c[3] = {0xC1u, 0xC2u, 0xC3u};
  const uint8_t payload_d[1] = {0xD1u};

  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 0u, 1u, payload_a, (uint32_t)sizeof(payload_a)), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 0u, 2u, payload_b, (uint32_t)sizeof(payload_b)), ZR_OK);

  zr_event_t ev0;
  ZR_ASSERT_TRUE(zr_event_queue_pop(&q, &ev0));
  ZR_ASSERT_EQ_U32(ev0.type, (uint32_t)ZR_EV_USER);
  ZR_ASSERT_EQ_U32(ev0.u.user.hdr.tag, 1u);

  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 0u, 3u, payload_c, (uint32_t)sizeof(payload_c)), ZR_OK);

  /* Without pad tracking, this can succeed and overwrite the queued payload_b bytes. */
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 0u, 4u, payload_d, (uint32_t)sizeof(payload_d)), ZR_ERR_LIMIT);

  zr_event_t ev1;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &ev1));
  ZR_ASSERT_EQ_U32(ev1.type, (uint32_t)ZR_EV_USER);
  ZR_ASSERT_EQ_U32(ev1.u.user.hdr.tag, 2u);
  ZR_ASSERT_EQ_U32(ev1.u.user.hdr.byte_len, (uint32_t)sizeof(payload_b));

  const uint8_t* view = NULL;
  uint32_t view_len = 0u;
  ZR_ASSERT_TRUE(zr_event_queue_user_payload_view(&q, &ev1, &view, &view_len));
  ZR_ASSERT_EQ_U32(view_len, (uint32_t)sizeof(payload_b));
  ZR_ASSERT_MEMEQ(view, payload_b, (uint32_t)sizeof(payload_b));
}
