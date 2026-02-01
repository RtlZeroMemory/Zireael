/*
  tests/unit/test_ring.c — Unit tests for util/zr_ring.h.
*/

#include "zr_test.h"

#include "util/zr_ring.h"

#include <stdint.h>

ZR_TEST_UNIT(ring_fifo_order_and_full_semantics) {
  uint32_t backing[3] = {0u, 0u, 0u};
  zr_ring_t r;
  ZR_ASSERT_EQ_U32(zr_ring_init(&r, backing, 3u, sizeof(uint32_t)), ZR_OK);

  const uint32_t a = 1u;
  const uint32_t b = 2u;
  const uint32_t c = 3u;
  const uint32_t d = 4u;

  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &a), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &b), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &c), ZR_OK);
  ZR_ASSERT_TRUE(zr_ring_is_full(&r));
  ZR_ASSERT_EQ_U32(zr_ring_len(&r), 3u);

  /* Full push must not mutate. */
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &d), ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(zr_ring_len(&r), 3u);

  uint32_t out = 0u;
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 1u);
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 2u);
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 3u);
  ZR_ASSERT_TRUE(zr_ring_is_empty(&r));
  ZR_ASSERT_TRUE(!zr_ring_pop(&r, &out));
}

ZR_TEST_UNIT(ring_wraparound) {
  uint32_t backing[2] = {0u, 0u};
  zr_ring_t r;
  ZR_ASSERT_EQ_U32(zr_ring_init(&r, backing, 2u, sizeof(uint32_t)), ZR_OK);

  const uint32_t a = 10u;
  const uint32_t b = 20u;
  const uint32_t c = 30u;

  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &a), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &b), ZR_OK);

  uint32_t out = 0u;
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 10u);

  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &c), ZR_OK);

  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 20u);
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 30u);
}

