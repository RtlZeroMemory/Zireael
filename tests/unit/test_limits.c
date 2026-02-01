/*
  tests/unit/test_limits.c — Unit tests for util/zr_caps.h.
*/

#include "zr_test.h"

#include "util/zr_caps.h"

ZR_TEST_UNIT(limits_default_and_validate) {
  zr_limits_t l = zr_limits_default();
  ZR_ASSERT_TRUE(l.arena_max_total_bytes != 0u);
  ZR_ASSERT_TRUE(l.arena_initial_bytes != 0u);
  ZR_ASSERT_TRUE(l.dl_max_total_bytes != 0u);
  ZR_ASSERT_TRUE(l.dl_max_cmds != 0u);
  ZR_ASSERT_TRUE(l.dl_max_strings != 0u);
  ZR_ASSERT_TRUE(l.dl_max_blobs != 0u);
  ZR_ASSERT_TRUE(l.dl_max_clip_depth != 0u);
  ZR_ASSERT_TRUE(l.dl_max_text_run_segments != 0u);
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_OK);
}

ZR_TEST_UNIT(limits_validate_rejects_zero_or_invalid) {
  zr_limits_t l = zr_limits_default();
  l.arena_max_total_bytes = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  l = zr_limits_default();
  l.arena_initial_bytes = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  l = zr_limits_default();
  l.arena_initial_bytes = l.arena_max_total_bytes + 1u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);

  l = zr_limits_default();
  l.dl_max_total_bytes = 0u;
  ZR_ASSERT_EQ_U32(zr_limits_validate(&l), ZR_ERR_INVALID_ARGUMENT);
}
