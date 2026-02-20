/*
  tests/unit/test_image_cache.c — Unit tests for Kitty image cache helpers.

  Why: Protocol-side cache behavior (lookup/LRU/placement metadata) must stay
  deterministic for stable frame-to-frame output.
*/

#include "zr_test.h"

#include "core/zr_image.h"

#include <stdint.h>
#include <string.h>

static void zr_slot_seed(zr_image_slot_t* slot, uint32_t kitty_id, uint32_t image_id, uint64_t hash, uint16_t px_w,
                         uint16_t px_h, uint64_t tick, uint8_t transmitted) {
  memset(slot, 0, sizeof(*slot));
  slot->kitty_id = kitty_id;
  slot->image_id = image_id;
  slot->content_hash = hash;
  slot->px_width = px_w;
  slot->px_height = px_h;
  slot->lru_tick = tick;
  slot->transmitted = transmitted;
}

ZR_TEST_UNIT(image_cache_state_init_and_begin_frame) {
  zr_image_state_t state;
  zr_image_state_init(&state);

  ZR_ASSERT_EQ_U32(state.slot_count, 0u);
  ZR_ASSERT_EQ_U32(state.next_kitty_id, 1u);
  ZR_ASSERT_EQ_U32(state.lru_tick, 0u);

  state.slot_count = 2u;
  state.slots[0].placed_this_frame = 1u;
  state.slots[1].placed_this_frame = 1u;

  zr_image_state_begin_frame(&state);
  ZR_ASSERT_EQ_U32(state.slots[0].placed_this_frame, 0u);
  ZR_ASSERT_EQ_U32(state.slots[1].placed_this_frame, 0u);
}

ZR_TEST_UNIT(image_cache_lookup_by_id_hash_and_hash_dims) {
  zr_image_state_t state;
  zr_image_state_init(&state);
  state.slot_count = 3u;

  zr_slot_seed(&state.slots[0], 10u, 111u, 0xAAA1u, 4u, 4u, 1u, 1u);
  zr_slot_seed(&state.slots[1], 11u, 222u, 0xBBB2u, 8u, 8u, 2u, 1u);
  zr_slot_seed(&state.slots[2], 12u, 333u, 0xCCC3u, 8u, 8u, 3u, 0u);

  ZR_ASSERT_TRUE(zr_image_cache_find_by_id_hash(&state, 111u, 0xAAA1u, 4u, 4u) == 0);
  ZR_ASSERT_TRUE(zr_image_cache_find_by_id_hash(&state, 111u, 0xAAA1u, 8u, 2u) == -1);
  ZR_ASSERT_TRUE(zr_image_cache_find_by_id_hash(&state, 333u, 0xCCC3u, 8u, 8u) == -1);
  ZR_ASSERT_TRUE(zr_image_cache_find_by_hash_dims(&state, 0xBBB2u, 8u, 8u) == 1);
  ZR_ASSERT_TRUE(zr_image_cache_find_by_hash_dims(&state, 0xCCC3u, 8u, 8u) == -1);
}

ZR_TEST_UNIT(image_cache_choose_slot_prefers_growth_empty_and_lru) {
  zr_image_state_t state;
  zr_image_state_init(&state);

  state.slot_count = 3u;
  ZR_ASSERT_EQ_U32(zr_image_cache_choose_slot(&state), 3u);

  state.slot_count = ZR_IMAGE_CACHE_SIZE;
  for (uint32_t i = 0u; i < state.slot_count; i++) {
    zr_slot_seed(&state.slots[i], i + 1u, i + 1u, (uint64_t)i, 1u, 1u, (uint64_t)(100u + i), 1u);
  }
  state.slots[17].transmitted = 0u;
  ZR_ASSERT_EQ_U32(zr_image_cache_choose_slot(&state), 17u);

  state.slots[17].transmitted = 1u;
  state.slots[22].lru_tick = 1u;
  ZR_ASSERT_EQ_U32(zr_image_cache_choose_slot(&state), 22u);
}

ZR_TEST_UNIT(image_cache_touch_and_set_placed_updates_metadata) {
  zr_image_state_t state;
  zr_image_state_init(&state);
  state.slot_count = 1u;
  zr_slot_seed(&state.slots[0], 77u, 200u, 0x1234u, 16u, 9u, 9u, 1u);
  state.lru_tick = 9u;

  zr_image_cache_touch(&state, 0u);
  ZR_ASSERT_EQ_U32(state.lru_tick, 10u);
  ZR_ASSERT_TRUE(state.slots[0].lru_tick == 10u);

  zr_image_cache_set_placed(&state, 0u, 4u, 5u, 6u, 7u, -1);
  ZR_ASSERT_EQ_U32(state.slots[0].placed_this_frame, 1u);
  ZR_ASSERT_EQ_U32(state.slots[0].dst_col, 4u);
  ZR_ASSERT_EQ_U32(state.slots[0].dst_row, 5u);
  ZR_ASSERT_EQ_U32(state.slots[0].dst_cols, 6u);
  ZR_ASSERT_EQ_U32(state.slots[0].dst_rows, 7u);
  ZR_ASSERT_TRUE(state.slots[0].z_layer == -1);
  ZR_ASSERT_EQ_U32(state.lru_tick, 11u);
}
