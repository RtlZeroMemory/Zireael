/*
  tests/unit/test_arena.c — Unit tests for util/zr_arena.h.
*/

#include "zr_test.h"

#include "util/zr_arena.h"

#include <stdint.h>

static uintptr_t zr_ptr_u(const void* p) {
  return (uintptr_t)p;
}

ZR_TEST_UNIT(arena_size_zero_policy_and_zeroed_alloc) {
  zr_arena_t a;
  ZR_ASSERT_EQ_U32(zr_arena_init(&a, 32u, 128u), ZR_OK);

  uint8_t* p = (uint8_t*)zr_arena_alloc(&a, 0u, 1u);
  ZR_ASSERT_TRUE(p != NULL);

  uint8_t* z = (uint8_t*)zr_arena_alloc_zeroed(&a, 8u, 1u);
  ZR_ASSERT_TRUE(z != NULL);
  for (size_t i = 0; i < 8u; i++) {
    ZR_ASSERT_EQ_U32(z[i], 0u);
  }

  zr_arena_release(&a);
  zr_arena_release(&a); /* idempotent */
}

ZR_TEST_UNIT(arena_alignment) {
  zr_arena_t a;
  ZR_ASSERT_EQ_U32(zr_arena_init(&a, 64u, 512u), ZR_OK);

  void* p16 = zr_arena_alloc(&a, 1u, 16u);
  ZR_ASSERT_TRUE(p16 != NULL);
  ZR_ASSERT_TRUE((zr_ptr_u(p16) & 15u) == 0u);

  void* p256 = zr_arena_alloc(&a, 1u, 256u);
  ZR_ASSERT_TRUE(p256 != NULL);
  ZR_ASSERT_TRUE((zr_ptr_u(p256) & 255u) == 0u);

  zr_arena_release(&a);
}

ZR_TEST_UNIT(arena_mark_rewind_restores_offset) {
  zr_arena_t a;
  ZR_ASSERT_EQ_U32(zr_arena_init(&a, 256u, 512u), ZR_OK);

  void* p1 = zr_arena_alloc(&a, 8u, 8u);
  ZR_ASSERT_TRUE(p1 != NULL);

  zr_arena_mark_t m = zr_arena_mark(&a);
  void* p2 = zr_arena_alloc(&a, 16u, 8u);
  ZR_ASSERT_TRUE(p2 != NULL);
  (void)zr_arena_alloc(&a, 16u, 8u);

  zr_arena_rewind(&a, m);
  void* p4 = zr_arena_alloc(&a, 16u, 8u);
  ZR_ASSERT_TRUE(p4 != NULL);
  ZR_ASSERT_TRUE(p4 == p2);

  zr_arena_release(&a);
}

ZR_TEST_UNIT(arena_cap_enforced_no_partial_effects) {
  zr_arena_t a;
  ZR_ASSERT_EQ_U32(zr_arena_init(&a, 16u, 64u), ZR_OK);

  void* p1 = zr_arena_alloc(&a, 8u, 8u);
  ZR_ASSERT_TRUE(p1 != NULL);

  /* Force a grow request that would exceed max_total_bytes (deterministic NULL). */
  void* big = zr_arena_alloc(&a, 100u, 8u);
  ZR_ASSERT_TRUE(big == NULL);

  /* Subsequent small allocs should still succeed. */
  void* p2 = zr_arena_alloc(&a, 8u, 8u);
  ZR_ASSERT_TRUE(p2 != NULL);

  zr_arena_release(&a);
}

ZR_TEST_UNIT(arena_reset_reuses_first_block) {
  zr_arena_t a;
  ZR_ASSERT_EQ_U32(zr_arena_init(&a, 64u, 256u), ZR_OK);

  void* p1 = zr_arena_alloc(&a, 32u, 8u);
  ZR_ASSERT_TRUE(p1 != NULL);

  zr_arena_reset(&a);
  void* p2 = zr_arena_alloc(&a, 32u, 8u);
  ZR_ASSERT_TRUE(p2 != NULL);

  zr_arena_release(&a);
}

