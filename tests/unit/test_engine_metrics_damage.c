/*
  tests/unit/test_engine_metrics_damage.c — Unit tests for damage metrics plumbing.

  Why: Verifies that engine_present populates the appended damage summary fields
  in zr_metrics_t deterministically (append-only ABI).
*/

#include "zr_test.h"

#include "core/zr_engine.h"

#include "unit/mock_platform.h"

#include <string.h>

extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;

ZR_TEST_UNIT(engine_metrics_damage_fields_update_on_present) {
  mock_plat_reset();
  mock_plat_set_size(4u, 1u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  /* Seed: a present with no drawlist should be a no-op diff. */
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);

  /* Apply a drawlist that changes exactly two cells ("Hi" at x=1). */
  ZR_ASSERT_EQ_U32(engine_submit_drawlist(e, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len), ZR_OK);
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);

  zr_metrics_t m;
  memset(&m, 0, sizeof(m));
  m.struct_size = (uint32_t)sizeof(zr_metrics_t);
  ZR_ASSERT_EQ_U32(engine_get_metrics(e, &m), ZR_OK);

  ZR_ASSERT_EQ_U32(m.damage_rects_last_frame, 1u);
  ZR_ASSERT_EQ_U32(m.damage_cells_last_frame, 2u);
  ZR_ASSERT_EQ_U32((uint32_t)m.damage_full_frame, 0u);

  engine_destroy(e);
}
