/*
  tests/unit/test_vec.c — Unit tests for util/zr_vec.h.

  Why: Validates fixed-capacity vector contracts including push/pop LIFO
  semantics, capacity limit enforcement without partial mutations, and
  the zero-capacity edge case.

  Scenarios tested:
    - Push fills vector to capacity; overflow returns ZR_ERR_LIMIT
    - Failed push does not mutate vector state
    - Pop returns elements in LIFO order
    - Pop on empty vector returns error
    - Zero-capacity vector allows NULL backing and rejects all pushes
*/

#include "zr_test.h"

#include "util/zr_vec.h"

#include <stdint.h>

/*
 * Test: vec_push_limit_no_mutate
 *
 * Scenario: When the vector is full, push returns ZR_ERR_LIMIT and does
 *           not modify the vector state (no partial effects).
 *
 * Arrange: Initialize 3-element vector, push 3 values to fill it.
 * Act:     Attempt to push a 4th value.
 * Assert:  Push returns ZR_ERR_LIMIT; length stays 3; last element unchanged.
 */
ZR_TEST_UNIT(vec_push_limit_no_mutate) {
  /* --- Arrange --- */
  uint32_t backing[3] = {0u, 0u, 0u};
  zr_vec_t v;
  ZR_ASSERT_EQ_U32(zr_vec_init(&v, backing, 3u, sizeof(uint32_t)), ZR_OK);

  const uint32_t a = 10u;
  const uint32_t b = 20u;
  const uint32_t c = 30u;
  const uint32_t d = 40u;

  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &a), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &b), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &c), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_vec_len(&v), 3u);

  /* --- Act: Push when full --- */
  const zr_result_t rc = zr_vec_push(&v, &d);

  /* --- Assert: Returns error, no mutation --- */
  ZR_ASSERT_EQ_U32(rc, ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(zr_vec_len(&v), 3u);
  ZR_ASSERT_EQ_U32(*(const uint32_t*)zr_vec_at_const(&v, 2u), 30u);
}

/*
 * Test: vec_pop
 *
 * Scenario: Pop returns elements in LIFO (last-in-first-out) order and
 *           fails gracefully when the vector is empty.
 *
 * Arrange: Initialize 2-element vector, push values 111 and 222.
 * Act:     Pop twice, then attempt a third pop.
 * Assert:  First pop returns 222, second returns 111, third fails.
 */
ZR_TEST_UNIT(vec_pop) {
  /* --- Arrange --- */
  uint32_t backing[2] = {0u, 0u};
  zr_vec_t v;
  ZR_ASSERT_EQ_U32(zr_vec_init(&v, backing, 2u, sizeof(uint32_t)), ZR_OK);

  const uint32_t a = 111u;
  const uint32_t b = 222u;
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &a), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &b), ZR_OK);

  /* --- Act & Assert: Pop returns LIFO order --- */
  uint32_t out = 0u;
  ZR_ASSERT_EQ_U32(zr_vec_pop(&v, &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out, 222u);
  ZR_ASSERT_EQ_U32(zr_vec_len(&v), 1u);

  ZR_ASSERT_EQ_U32(zr_vec_pop(&v, &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out, 111u);
  ZR_ASSERT_EQ_U32(zr_vec_len(&v), 0u);

  /* --- Act & Assert: Pop on empty fails --- */
  ZR_ASSERT_TRUE(zr_vec_pop(&v, &out) != ZR_OK);
}

/*
 * Test: vec_zero_cap_allows_null_backing
 *
 * Scenario: A zero-capacity vector can be initialized with NULL backing
 *           and correctly reports its empty state.
 *
 * Arrange: Initialize vector with NULL backing and capacity 0.
 * Act:     Check length/capacity, attempt push.
 * Assert:  Length and capacity are 0; push returns ZR_ERR_LIMIT.
 */
ZR_TEST_UNIT(vec_zero_cap_allows_null_backing) {
  /* --- Arrange --- */
  zr_vec_t v;
  ZR_ASSERT_EQ_U32(zr_vec_init(&v, NULL, 0u, sizeof(uint32_t)), ZR_OK);

  /* --- Assert: Reports empty state --- */
  ZR_ASSERT_EQ_U32(zr_vec_len(&v), 0u);
  ZR_ASSERT_EQ_U32(zr_vec_cap(&v), 0u);

  /* --- Act & Assert: Push fails gracefully --- */
  const uint32_t x = 1u;
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &x), ZR_ERR_LIMIT);
}
