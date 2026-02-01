/*
  tests/unit/test_user_event_injection.c — Unit tests for thread-safe user event injection.
*/

#include "zr_test.h"

#include "core/zr_event_queue.h"

#include <string.h>

ZR_TEST_UNIT(user_event_injection_copies_payload_bytes) {
  zr_event_t storage[4];
  uint8_t user_bytes[16];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 4u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  uint8_t payload[3] = {1u, 2u, 3u};
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 7u, 0xBEEFu, payload, (uint32_t)sizeof(payload)), ZR_OK);

  /* Mutate caller buffer; queue must have copied. */
  payload[0] = 9u;
  payload[1] = 9u;
  payload[2] = 9u;

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

ZR_TEST_UNIT(user_event_injection_enforces_payload_cap_without_partial_enqueue) {
  zr_event_t storage[4];
  uint8_t user_bytes[4];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 4u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  const uint8_t payload4[4] = {1u, 2u, 3u, 4u};
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 0u, 1u, payload4, 4u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  const uint8_t payload1[1] = {9u};
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 0u, 2u, payload1, 1u), ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);
}

ZR_TEST_UNIT(user_event_injection_rejects_when_queue_full) {
  zr_event_t storage[1];
  uint8_t user_bytes[8];
  zr_event_queue_t q;
  ZR_ASSERT_EQ_U32(zr_event_queue_init(&q, storage, 1u, user_bytes, (uint32_t)sizeof(user_bytes)), ZR_OK);

  zr_event_t key;
  memset(&key, 0, sizeof(key));
  key.type = ZR_EV_KEY;
  key.u.key.key = (uint32_t)ZR_KEY_TAB;
  key.u.key.action = (uint32_t)ZR_KEY_ACTION_DOWN;
  ZR_ASSERT_EQ_U32(zr_event_queue_push(&q, &key), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  const uint8_t payload[1] = {0u};
  ZR_ASSERT_EQ_U32(zr_event_queue_post_user(&q, 0u, 1u, payload, 1u), ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(zr_event_queue_count(&q), 1u);

  zr_event_t head;
  ZR_ASSERT_TRUE(zr_event_queue_peek(&q, &head));
  ZR_ASSERT_EQ_U32(head.type, (uint32_t)ZR_EV_KEY);
}

