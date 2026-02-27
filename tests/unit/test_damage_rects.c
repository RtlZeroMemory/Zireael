/*
  tests/unit/test_damage_rects.c — Unit tests for core damage rectangle tracking.

  Why: Ensures zr_damage_t coalesces deterministically, stays cap-bounded, and
  reports stable summary counts used by public metrics.
*/

#include "zr_test.h"

#include "core/zr_damage.h"

ZR_TEST_UNIT(damage_merges_vertical_spans_with_same_extent) {
  zr_damage_rect_t storage[8];
  zr_damage_t d;
  zr_damage_begin_frame(&d, storage, 8u, 10u, 10u);

  zr_damage_add_span(&d, 0u, 1u, 3u);
  zr_damage_add_span(&d, 1u, 1u, 3u);

  ZR_ASSERT_EQ_U32(d.full_frame, 0u);
  ZR_ASSERT_EQ_U32(d.rect_count, 1u);
  ZR_ASSERT_EQ_U32(d.rects[0].x0, 1u);
  ZR_ASSERT_EQ_U32(d.rects[0].x1, 3u);
  ZR_ASSERT_EQ_U32(d.rects[0].y0, 0u);
  ZR_ASSERT_EQ_U32(d.rects[0].y1, 1u);
  ZR_ASSERT_EQ_U32(zr_damage_cells(&d), 6u);
}

ZR_TEST_UNIT(damage_does_not_merge_different_spans) {
  zr_damage_rect_t storage[8];
  zr_damage_t d;
  zr_damage_begin_frame(&d, storage, 8u, 10u, 10u);

  zr_damage_add_span(&d, 0u, 1u, 3u);
  zr_damage_add_span(&d, 1u, 4u, 5u);

  ZR_ASSERT_EQ_U32(d.full_frame, 0u);
  ZR_ASSERT_EQ_U32(d.rect_count, 2u);
  ZR_ASSERT_EQ_U32(zr_damage_cells(&d), (uint32_t)(3u + 2u));
}

ZR_TEST_UNIT(damage_marks_full_frame_on_rect_cap_overflow) {
  zr_damage_rect_t storage[1];
  zr_damage_t d;
  zr_damage_begin_frame(&d, storage, 1u, 5u, 4u);

  zr_damage_add_span(&d, 0u, 0u, 0u);
  zr_damage_add_span(&d, 0u, 2u, 2u);

  ZR_ASSERT_EQ_U32(d.full_frame, 1u);
  ZR_ASSERT_EQ_U32(d.rect_count, 1u);
  ZR_ASSERT_EQ_U32(d.rects[0].x0, 0u);
  ZR_ASSERT_EQ_U32(d.rects[0].y0, 0u);
  ZR_ASSERT_EQ_U32(d.rects[0].x1, 4u);
  ZR_ASSERT_EQ_U32(d.rects[0].y1, 3u);
  ZR_ASSERT_EQ_U32(zr_damage_cells(&d), 20u);
}
