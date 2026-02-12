/*
  tests/unit/test_engine_restore_hooks.c — Engine abort/exit restore hook hardening tests.

  Why: Verifies the engine attempts best-effort terminal restore through
  assert-cleanup and process-exit hook paths in addition to normal destroy.
*/

#include "zr_test.h"

#include "core/zr_engine.h"
#include "util/zr_assert.h"

#include "unit/mock_platform.h"

#include <stdint.h>

/* Unit-only hooks exported by zr_engine.c when ZR_ENGINE_TESTING=1. */
extern void     zr_engine_test_reset_restore_counters(void);
extern uint32_t zr_engine_test_restore_attempts(void);
extern uint32_t zr_engine_test_restore_abort_calls(void);
extern uint32_t zr_engine_test_restore_exit_calls(void);
extern void     zr_engine_test_invoke_exit_restore_hook(void);

ZR_TEST_UNIT(engine_restore_hook_runs_on_assert_cleanup_path) {
  mock_plat_reset();

  zr_engine_config_t cfg = zr_engine_config_default();
  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_engine_test_reset_restore_counters();

  zr_assert_invoke_cleanup_hook_for_test();

  ZR_ASSERT_EQ_U32(zr_engine_test_restore_abort_calls(), 1u);
  ZR_ASSERT_EQ_U32(zr_engine_test_restore_exit_calls(), 0u);
  ZR_ASSERT_EQ_U32(zr_engine_test_restore_attempts(), 1u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_restore_hook_runs_on_exit_path) {
  mock_plat_reset();

  zr_engine_config_t cfg = zr_engine_config_default();
  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_engine_test_reset_restore_counters();

  zr_engine_test_invoke_exit_restore_hook();

  ZR_ASSERT_EQ_U32(zr_engine_test_restore_abort_calls(), 0u);
  ZR_ASSERT_EQ_U32(zr_engine_test_restore_exit_calls(), 1u);
  ZR_ASSERT_EQ_U32(zr_engine_test_restore_attempts(), 1u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_restore_hook_is_cleared_after_last_engine_destroy) {
  mock_plat_reset();

  zr_engine_config_t cfg = zr_engine_config_default();
  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);
  engine_destroy(e);

  zr_engine_test_reset_restore_counters();

  zr_assert_invoke_cleanup_hook_for_test();

  ZR_ASSERT_EQ_U32(zr_engine_test_restore_abort_calls(), 0u);
  ZR_ASSERT_EQ_U32(zr_engine_test_restore_exit_calls(), 0u);
  ZR_ASSERT_EQ_U32(zr_engine_test_restore_attempts(), 0u);
}
