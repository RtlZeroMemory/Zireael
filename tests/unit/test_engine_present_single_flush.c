/*
  tests/unit/test_engine_present_single_flush.c — Engine present single-flush contract.

  Why: Validates that engine_present emits terminal bytes via exactly one
  plat_write_output call on success, and does not flush at all when diff output
  cannot fit in the engine-owned per-frame output buffer.
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"

#include "unit/mock_platform.h"

extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;

ZR_TEST_UNIT(engine_present_single_flush_on_success) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(engine_submit_drawlist(e, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len) == ZR_OK);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_OK);

  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);
  ZR_ASSERT_TRUE(mock_plat_bytes_written_total() != 0u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_no_flush_on_limit_error) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 8u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(engine_submit_drawlist(e, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len) == ZR_OK);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 0u);

  engine_destroy(e);
}
