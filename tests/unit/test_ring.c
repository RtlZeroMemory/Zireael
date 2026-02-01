/*
  tests/unit/test_ring.c — Unit tests for util/zr_ring.h.

  Why: Validates ring buffer FIFO semantics, wraparound behavior, capacity
  enforcement without partial mutations, and the zero-capacity edge case.

  Scenarios tested:
    - FIFO ordering: elements pop in the same order they were pushed
    - Full state detected; push on full returns ZR_ERR_LIMIT without mutation
    - Pop on empty returns false
    - Wraparound: push/pop interleaving correctly wraps head/tail pointers
    - Zero-capacity ring allows NULL backing and handles empty state
*/

#include "zr_test.h"

#include "util/zr_ring.h"

#include <stdint.h>

/*
 * Test: ring_fifo_order_and_full_semantics
 *
 * Scenario: Ring buffer maintains FIFO order and rejects pushes when full
 *           without corrupting state.
 *
 * Arrange: Initialize 3-element ring buffer.
 * Act:     Push 3 values (fills buffer), attempt 4th push, then pop all.
 * Assert:  4th push fails with ZR_ERR_LIMIT; pops return values in FIFO order;
 *          pop on empty returns false.
 */
ZR_TEST_UNIT(ring_fifo_order_and_full_semantics) {
  /* --- Arrange --- */
  uint32_t backing[3] = {0u, 0u, 0u};
  zr_ring_t r;
  ZR_ASSERT_EQ_U32(zr_ring_init(&r, backing, 3u, sizeof(uint32_t)), ZR_OK);

  const uint32_t a = 1u;
  const uint32_t b = 2u;
  const uint32_t c = 3u;
  const uint32_t d = 4u;

  /* --- Act: Fill buffer --- */
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &a), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &b), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &c), ZR_OK);
  ZR_ASSERT_TRUE(zr_ring_is_full(&r));
  ZR_ASSERT_EQ_U32(zr_ring_len(&r), 3u);

  /* --- Act & Assert: Push on full fails without mutation --- */
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &d), ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(zr_ring_len(&r), 3u);

  /* --- Assert: Pops return FIFO order --- */
  uint32_t out = 0u;
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 1u);
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 2u);
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 3u);

  /* --- Assert: Empty state, pop fails --- */
  ZR_ASSERT_TRUE(zr_ring_is_empty(&r));
  ZR_ASSERT_TRUE(!zr_ring_pop(&r, &out));
}

/*
 * Test: ring_wraparound
 *
 * Scenario: Ring buffer correctly handles wraparound when head/tail pointers
 *           cycle past the end of the backing array.
 *
 * Arrange: Initialize 2-element ring buffer.
 * Act:     Push 10, push 20 (full), pop 10, push 30, pop 20, pop 30.
 * Assert:  All pops return expected values in FIFO order despite wraparound.
 */
ZR_TEST_UNIT(ring_wraparound) {
  /* --- Arrange --- */
  uint32_t backing[2] = {0u, 0u};
  zr_ring_t r;
  ZR_ASSERT_EQ_U32(zr_ring_init(&r, backing, 2u, sizeof(uint32_t)), ZR_OK);

  const uint32_t a = 10u;
  const uint32_t b = 20u;
  const uint32_t c = 30u;

  /* --- Act: Fill, pop one, push another (causes wraparound) --- */
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &a), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &b), ZR_OK);

  uint32_t out = 0u;
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 10u);

  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &c), ZR_OK);

  /* --- Assert: Remaining pops return FIFO order --- */
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 20u);
  ZR_ASSERT_TRUE(zr_ring_pop(&r, &out));
  ZR_ASSERT_EQ_U32(out, 30u);
}

/*
 * Test: ring_zero_cap_allows_null_backing
 *
 * Scenario: A zero-capacity ring buffer can be initialized with NULL backing
 *           and handles empty state correctly.
 *
 * Arrange: Initialize ring with NULL backing and capacity 0.
 * Act:     Check empty/full state, attempt push and pop.
 * Assert:  Reports empty (not full); push returns ZR_ERR_LIMIT; pop returns false.
 */
ZR_TEST_UNIT(ring_zero_cap_allows_null_backing) {
  /* --- Arrange --- */
  zr_ring_t r;
  ZR_ASSERT_EQ_U32(zr_ring_init(&r, NULL, 0u, sizeof(uint32_t)), ZR_OK);

  /* --- Assert: Reports empty, not full --- */
  ZR_ASSERT_TRUE(zr_ring_is_empty(&r));
  ZR_ASSERT_TRUE(!zr_ring_is_full(&r));

  /* --- Act & Assert: Push fails --- */
  const uint32_t x = 1u;
  ZR_ASSERT_EQ_U32(zr_ring_push(&r, &x), ZR_ERR_LIMIT);

  /* --- Act & Assert: Pop fails --- */
  uint32_t out = 0u;
  ZR_ASSERT_TRUE(!zr_ring_pop(&r, &out));
}
