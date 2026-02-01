/*
  tests/unit/test_engine_submit_drawlist_no_partial_effects.c — Drawlist submit no-partial-effects.

  Why: Validates the locked contract that engine_submit_drawlist performs full
  validation before mutating the engine's next framebuffer. If submission fails,
  the next framebuffer must be unchanged.
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"

#include "unit/mock_platform.h"

#include <string.h>

extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;

static size_t zr_capture_present_bytes(zr_engine_t* e, uint8_t* out, size_t out_cap, size_t* out_len) {
  if (!out_len) {
    return 0u;
  }
  *out_len = 0u;
  mock_plat_clear_writes();
  if (engine_present(e) != ZR_OK) {
    return 0u;
  }
  const size_t n = mock_plat_last_write_copy(out, out_cap);
  *out_len = n;
  return n;
}

ZR_TEST_UNIT(engine_submit_drawlist_failure_does_not_mutate_next_framebuffer) {
  uint8_t a_bytes[4096];
  uint8_t b_bytes[4096];
  size_t a_len = 0u;
  size_t b_len = 0u;

  /* Baseline: submit A, then present. */
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e1 = NULL;
  ZR_ASSERT_TRUE(engine_create(&e1, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e1 != NULL);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e1, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len) == ZR_OK);
  ZR_ASSERT_TRUE(zr_capture_present_bytes(e1, a_bytes, sizeof(a_bytes), &a_len) != 0u);
  engine_destroy(e1);

  /* Candidate: submit A, then a failing drawlist; present should match baseline. */
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_t* e2 = NULL;
  ZR_ASSERT_TRUE(engine_create(&e2, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e2 != NULL);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e2, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len) == ZR_OK);

  uint8_t bad[256];
  ZR_ASSERT_TRUE(zr_test_dl_fixture1_len <= sizeof(bad));
  memcpy(bad, zr_test_dl_fixture1, zr_test_dl_fixture1_len);
  bad[0] ^= 0xFFu; /* break magic deterministically */

  ZR_ASSERT_TRUE(engine_submit_drawlist(e2, bad, (int)zr_test_dl_fixture1_len) != ZR_OK);
  ZR_ASSERT_TRUE(zr_capture_present_bytes(e2, b_bytes, sizeof(b_bytes), &b_len) != 0u);
  engine_destroy(e2);

  ZR_ASSERT_TRUE(a_len == b_len);
  ZR_ASSERT_MEMEQ(a_bytes, b_bytes, a_len);
}
