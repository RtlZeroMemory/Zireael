/*
  tests/unit/test_arena.c — Unit tests for util/zr_arena.h.

  Why: Validates arena allocator contracts including alignment guarantees,
  mark/rewind semantics, capacity enforcement, and idempotent release.

  Scenarios tested:
    - Zero-size allocation returns valid pointer (sentinel)
    - Zeroed allocation initializes memory to zero
    - Alignment constraints are honored for various power-of-two alignments
    - Mark/rewind restores allocation state correctly
    - Capacity limits are enforced without partial side effects
    - Reset reuses first block for subsequent allocations
    - Release is idempotent (can be called multiple times safely)
*/

#include "zr_test.h"

#include "util/zr_arena.h"

#include <stdint.h>

/* Convert pointer to uintptr_t for alignment checks. */
static uintptr_t zr_ptr_u(const void* p) {
  return (uintptr_t)p;
}

/*
 * Test: arena_size_zero_policy_and_zeroed_alloc
 *
 * Scenario: Zero-size allocations return a valid pointer (not NULL),
 *           and zeroed allocations initialize memory to zero bytes.
 *           Release must be idempotent.
 *
 * Arrange: Initialize arena with 32-byte initial block, 128-byte max.
 * Act:     Allocate 0 bytes, then allocate 8 zeroed bytes.
 * Assert:  Both return non-NULL; zeroed allocation contains all zeros.
 *          Double-release does not crash.
 */
ZR_TEST_UNIT(arena_size_zero_policy_and_zeroed_alloc) {
  /* --- Arrange --- */
  zr_arena_t a;
  ZR_ASSERT_EQ_U32(zr_arena_init(&a, 32u, 128u), ZR_OK);

  /* --- Act: Zero-size allocation --- */
  uint8_t* p = (uint8_t*)zr_arena_alloc(&a, 0u, 1u);

  /* --- Assert: Zero-size returns valid pointer --- */
  ZR_ASSERT_TRUE(p != NULL);

  /* --- Act: Zeroed allocation --- */
  uint8_t* z = (uint8_t*)zr_arena_alloc_zeroed(&a, 8u, 1u);

  /* --- Assert: Zeroed allocation is all zeros --- */
  ZR_ASSERT_TRUE(z != NULL);
  for (size_t i = 0; i < 8u; i++) {
    ZR_ASSERT_EQ_U32(z[i], 0u);
  }

  /* --- Cleanup: Release is idempotent --- */
  zr_arena_release(&a);
  zr_arena_release(&a);
}

/*
 * Test: arena_alignment
 *
 * Scenario: Allocations with specific alignment requirements return
 *           pointers that satisfy those alignment constraints.
 *
 * Arrange: Initialize arena with 64-byte initial, 512-byte max.
 * Act:     Allocate 1 byte with 16-byte alignment, then 1 byte with 256-byte alignment.
 * Assert:  Returned pointers are aligned to the requested boundaries.
 */
ZR_TEST_UNIT(arena_alignment) {
  /* --- Arrange --- */
  zr_arena_t a;
  ZR_ASSERT_EQ_U32(zr_arena_init(&a, 64u, 512u), ZR_OK);

  /* --- Act & Assert: 16-byte alignment --- */
  void* p16 = zr_arena_alloc(&a, 1u, 16u);
  ZR_ASSERT_TRUE(p16 != NULL);
  ZR_ASSERT_TRUE((zr_ptr_u(p16) & 15u) == 0u);

  /* --- Act & Assert: 256-byte alignment --- */
  void* p256 = zr_arena_alloc(&a, 1u, 256u);
  ZR_ASSERT_TRUE(p256 != NULL);
  ZR_ASSERT_TRUE((zr_ptr_u(p256) & 255u) == 0u);

  /* --- Cleanup --- */
  zr_arena_release(&a);
}

/*
 * Test: arena_mark_rewind_restores_offset
 *
 * Scenario: Mark captures arena state; rewind restores it, allowing
 *           subsequent allocations to reuse the same memory region.
 *
 * Arrange: Initialize arena, allocate 8 bytes as baseline.
 * Act:     Mark state, allocate 16 bytes twice, rewind to mark,
 *          allocate 16 bytes again.
 * Assert:  Post-rewind allocation returns the same pointer as pre-rewind.
 */
ZR_TEST_UNIT(arena_mark_rewind_restores_offset) {
  /* --- Arrange --- */
  zr_arena_t a;
  ZR_ASSERT_EQ_U32(zr_arena_init(&a, 256u, 512u), ZR_OK);

  void* p1 = zr_arena_alloc(&a, 8u, 8u);
  ZR_ASSERT_TRUE(p1 != NULL);

  /* --- Act: Mark, allocate, rewind --- */
  zr_arena_mark_t m = zr_arena_mark(&a);
  void* p2 = zr_arena_alloc(&a, 16u, 8u);
  ZR_ASSERT_TRUE(p2 != NULL);
  (void)zr_arena_alloc(&a, 16u, 8u); /* Additional allocation to advance offset */

  zr_arena_rewind(&a, m);
  void* p4 = zr_arena_alloc(&a, 16u, 8u);

  /* --- Assert: Rewind restores allocation point --- */
  ZR_ASSERT_TRUE(p4 != NULL);
  ZR_ASSERT_TRUE(p4 == p2);

  /* --- Cleanup --- */
  zr_arena_release(&a);
}

/*
 * Test: arena_cap_enforced_no_partial_effects
 *
 * Scenario: Allocation requests exceeding max capacity return NULL
 *           without corrupting arena state; subsequent valid allocations
 *           continue to work.
 *
 * Arrange: Initialize arena with 16-byte initial, 64-byte max.
 * Act:     Allocate 8 bytes (succeeds), then 100 bytes (exceeds cap, fails),
 *          then 8 bytes again.
 * Assert:  Oversized allocation returns NULL; arena remains usable.
 */
ZR_TEST_UNIT(arena_cap_enforced_no_partial_effects) {
  /* --- Arrange --- */
  zr_arena_t a;
  ZR_ASSERT_EQ_U32(zr_arena_init(&a, 16u, 64u), ZR_OK);

  void* p1 = zr_arena_alloc(&a, 8u, 8u);
  ZR_ASSERT_TRUE(p1 != NULL);

  /* --- Act: Attempt allocation exceeding max_total_bytes --- */
  void* big = zr_arena_alloc(&a, 100u, 8u);

  /* --- Assert: Returns NULL, arena not corrupted --- */
  ZR_ASSERT_TRUE(big == NULL);

  /* --- Act & Assert: Subsequent valid allocation succeeds --- */
  void* p2 = zr_arena_alloc(&a, 8u, 8u);
  ZR_ASSERT_TRUE(p2 != NULL);

  /* --- Cleanup --- */
  zr_arena_release(&a);
}

/*
 * Test: arena_reset_reuses_first_block
 *
 * Scenario: Reset clears all allocations but keeps the first block,
 *           allowing memory reuse without reallocation.
 *
 * Arrange: Initialize arena with 64-byte initial block.
 * Act:     Allocate 32 bytes, reset, allocate 32 bytes again.
 * Assert:  Both allocations succeed (block is reused after reset).
 */
ZR_TEST_UNIT(arena_reset_reuses_first_block) {
  /* --- Arrange --- */
  zr_arena_t a;
  ZR_ASSERT_EQ_U32(zr_arena_init(&a, 64u, 256u), ZR_OK);

  /* --- Act: First allocation --- */
  void* p1 = zr_arena_alloc(&a, 32u, 8u);
  ZR_ASSERT_TRUE(p1 != NULL);

  /* --- Act: Reset and reallocate --- */
  zr_arena_reset(&a);
  void* p2 = zr_arena_alloc(&a, 32u, 8u);

  /* --- Assert: Second allocation succeeds (block reused) --- */
  ZR_ASSERT_TRUE(p2 != NULL);

  /* --- Cleanup --- */
  zr_arena_release(&a);
}

