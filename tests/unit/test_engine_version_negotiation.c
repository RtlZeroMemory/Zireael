/*
  tests/unit/test_engine_version_negotiation.c — Engine ABI/version negotiation.

  Why: Ensures engine_create enforces pinned ABI/binary format versions and
  leaves out_engine as NULL on ZR_ERR_UNSUPPORTED negotiation failures.
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"

#include "unit/mock_platform.h"

ZR_TEST_UNIT(engine_create_rejects_mismatched_abi_major_and_leaves_out_null) {
  mock_plat_reset();

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.requested_engine_abi_major = cfg.requested_engine_abi_major + 1u;

  zr_engine_t* e = (zr_engine_t*)0x1;
  const zr_result_t rc = engine_create(&e, &cfg);
  ZR_ASSERT_TRUE(rc == ZR_ERR_UNSUPPORTED);
  ZR_ASSERT_TRUE(e == NULL);
}

ZR_TEST_UNIT(engine_config_accepts_drawlist_v4) {
  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.requested_drawlist_version = ZR_DRAWLIST_VERSION_V4;
  ZR_ASSERT_EQ_U32(zr_engine_config_validate(&cfg), ZR_OK);
}

ZR_TEST_UNIT(engine_config_rejects_unknown_drawlist_version) {
  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.requested_drawlist_version = 999u;
  ZR_ASSERT_EQ_U32(zr_engine_config_validate(&cfg), ZR_ERR_UNSUPPORTED);
}
