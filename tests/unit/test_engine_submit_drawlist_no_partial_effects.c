/*
  tests/unit/test_engine_submit_drawlist_no_partial_effects.c — Drawlist submit no-partial-effects.

  Why: Validates the locked contract that engine_submit_drawlist performs full
  validation before mutating the engine's next framebuffer. If submission fails,
  the next framebuffer must be unchanged.
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"
#include "core/zr_version.h"
#include "zr/zr_drawlist.h"

#include "unit/mock_platform.h"

#include <string.h>

extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;
extern const uint8_t zr_test_dl_fixture3[];
extern const size_t zr_test_dl_fixture3_len;
extern const uint8_t zr_test_dl_fixture5_v2_cursor[];
extern const size_t zr_test_dl_fixture5_v2_cursor_len;

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

static uint8_t zr_contains_subseq(const uint8_t* hay, size_t hay_len, const uint8_t* needle, size_t needle_len) {
  if (!hay || !needle || needle_len == 0u || needle_len > hay_len) {
    return 0u;
  }
  for (size_t i = 0u; i + needle_len <= hay_len; i++) {
    if (memcmp(hay + i, needle, needle_len) == 0) {
      return 1u;
    }
  }
  return 0u;
}

static uint32_t zr_align4_u32(uint32_t n) {
  return (n + 3u) & ~3u;
}

static void zr_write_u16le(uint8_t* out, size_t* at, uint16_t v) {
  out[(*at)++] = (uint8_t)(v & 0xFFu);
  out[(*at)++] = (uint8_t)((v >> 8u) & 0xFFu);
}

static void zr_write_u32le(uint8_t* out, size_t* at, uint32_t v) {
  out[(*at)++] = (uint8_t)(v & 0xFFu);
  out[(*at)++] = (uint8_t)((v >> 8u) & 0xFFu);
  out[(*at)++] = (uint8_t)((v >> 16u) & 0xFFu);
  out[(*at)++] = (uint8_t)((v >> 24u) & 0xFFu);
}

static void zr_write_cmd_header(uint8_t* out, size_t* at, uint16_t opcode, uint32_t size) {
  zr_write_u16le(out, at, opcode);
  zr_write_u16le(out, at, 0u);
  zr_write_u32le(out, at, size);
}

static void zr_write_v6_header(uint8_t* out, size_t* at, uint32_t total_size, uint32_t cmd_bytes, uint32_t cmd_count) {
  zr_write_u32le(out, at, 0x4C44525Au);
  zr_write_u32le(out, at, ZR_DRAWLIST_VERSION_V1);
  zr_write_u32le(out, at, 64u);
  zr_write_u32le(out, at, total_size);
  zr_write_u32le(out, at, 64u);
  zr_write_u32le(out, at, cmd_bytes);
  zr_write_u32le(out, at, cmd_count);
  zr_write_u32le(out, at, 0u);
  zr_write_u32le(out, at, 0u);
  zr_write_u32le(out, at, 0u);
  zr_write_u32le(out, at, 0u);
  zr_write_u32le(out, at, 0u);
  zr_write_u32le(out, at, 0u);
  zr_write_u32le(out, at, 0u);
  zr_write_u32le(out, at, 0u);
  zr_write_u32le(out, at, 0u);
}

static size_t zr_make_dl_def_string(uint8_t* out, size_t out_cap, uint32_t id, const uint8_t* bytes, uint32_t len) {
  const uint32_t padded = zr_align4_u32(len);
  const uint32_t def_size = 8u + 8u + padded;
  const uint32_t cmd_bytes = 8u + def_size;
  const uint32_t total_size = 64u + cmd_bytes;
  size_t at = 0u;
  if (!out || !bytes || out_cap < (size_t)total_size) {
    return 0u;
  }
  memset(out, 0, out_cap);
  zr_write_v6_header(out, &at, total_size, cmd_bytes, 2u);
  zr_write_cmd_header(out, &at, ZR_DL_OP_CLEAR, 8u);
  zr_write_cmd_header(out, &at, ZR_DL_OP_DEF_STRING, def_size);
  zr_write_u32le(out, &at, id);
  zr_write_u32le(out, &at, len);
  memcpy(out + at, bytes, len);
  at += len;
  for (uint32_t i = len; i < padded; i++) {
    out[at++] = 0u;
  }
  return at;
}

static size_t zr_make_dl_free_string(uint8_t* out, size_t out_cap, uint32_t id) {
  const uint32_t cmd_bytes = 8u + 12u;
  const uint32_t total_size = 64u + cmd_bytes;
  size_t at = 0u;
  if (!out || out_cap < (size_t)total_size) {
    return 0u;
  }
  memset(out, 0, out_cap);
  zr_write_v6_header(out, &at, total_size, cmd_bytes, 2u);
  zr_write_cmd_header(out, &at, ZR_DL_OP_CLEAR, 8u);
  zr_write_cmd_header(out, &at, ZR_DL_OP_FREE_STRING, 12u);
  zr_write_u32le(out, &at, id);
  return at;
}

static size_t zr_make_dl_draw_text(uint8_t* out, size_t out_cap, uint32_t id, uint32_t byte_len) {
  const uint32_t cmd_bytes = 8u + 60u;
  const uint32_t total_size = 64u + cmd_bytes;
  size_t at = 0u;
  if (!out || out_cap < (size_t)total_size) {
    return 0u;
  }
  memset(out, 0, out_cap);
  zr_write_v6_header(out, &at, total_size, cmd_bytes, 2u);
  zr_write_cmd_header(out, &at, ZR_DL_OP_CLEAR, 8u);
  zr_write_cmd_header(out, &at, ZR_DL_OP_DRAW_TEXT, 60u);
  zr_write_u32le(out, &at, 0u);
  zr_write_u32le(out, &at, 0u);
  zr_write_u32le(out, &at, id);
  zr_write_u32le(out, &at, 0u);
  zr_write_u32le(out, &at, byte_len);
  zr_write_u32le(out, &at, 0u);
  zr_write_u32le(out, &at, 0u);
  zr_write_u32le(out, &at, 0u);
  zr_write_u32le(out, &at, 0u);
  zr_write_u32le(out, &at, 0u);
  zr_write_u32le(out, &at, 0u);
  zr_write_u32le(out, &at, 0u);
  zr_write_u32le(out, &at, 0u);
  return at;
}

static size_t zr_make_dl_invalid_blit_rect(uint8_t* out, size_t out_cap) {
  const uint32_t cmd_bytes = 8u + 32u;
  const uint32_t total_size = 64u + cmd_bytes;
  size_t at = 0u;
  if (!out || out_cap < (size_t)total_size) {
    return 0u;
  }
  memset(out, 0, out_cap);
  zr_write_v6_header(out, &at, total_size, cmd_bytes, 2u);
  zr_write_cmd_header(out, &at, ZR_DL_OP_CLEAR, 8u);
  zr_write_cmd_header(out, &at, ZR_DL_OP_BLIT_RECT, 32u);
  zr_write_u32le(out, &at, 9u); /* src_x: out-of-bounds for 10x4 framebuffer with w=2 */
  zr_write_u32le(out, &at, 0u); /* src_y */
  zr_write_u32le(out, &at, 2u); /* w */
  zr_write_u32le(out, &at, 1u); /* h */
  zr_write_u32le(out, &at, 0u); /* dst_x */
  zr_write_u32le(out, &at, 0u); /* dst_y */
  return at;
}

ZR_TEST_UNIT(engine_submit_drawlist_rejects_negotiated_version_mismatch) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  uint8_t legacy_dl[256];
  ZR_ASSERT_TRUE(zr_test_dl_fixture5_v2_cursor_len <= sizeof(legacy_dl));
  memcpy(legacy_dl, zr_test_dl_fixture5_v2_cursor, zr_test_dl_fixture5_v2_cursor_len);
  legacy_dl[4] = 5u;
  legacy_dl[5] = 0u;
  legacy_dl[6] = 0u;
  legacy_dl[7] = 0u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e, legacy_dl, (int)zr_test_dl_fixture5_v2_cursor_len) == ZR_ERR_UNSUPPORTED);
  engine_destroy(e);
}

ZR_TEST_UNIT(engine_submit_drawlist_version_mismatch_has_no_partial_effects) {
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

  /* Candidate: submit A, then a mismatched-version drawlist; present should match baseline. */
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_t* e2 = NULL;
  uint8_t legacy_dl[256];
  ZR_ASSERT_TRUE(zr_test_dl_fixture5_v2_cursor_len <= sizeof(legacy_dl));
  memcpy(legacy_dl, zr_test_dl_fixture5_v2_cursor, zr_test_dl_fixture5_v2_cursor_len);
  legacy_dl[4] = 5u;
  legacy_dl[5] = 0u;
  legacy_dl[6] = 0u;
  legacy_dl[7] = 0u;
  ZR_ASSERT_TRUE(engine_create(&e2, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e2 != NULL);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e2, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len) == ZR_OK);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e2, legacy_dl, (int)zr_test_dl_fixture5_v2_cursor_len) == ZR_ERR_UNSUPPORTED);
  ZR_ASSERT_TRUE(zr_capture_present_bytes(e2, b_bytes, sizeof(b_bytes), &b_len) != 0u);
  engine_destroy(e2);

  ZR_ASSERT_TRUE(a_len == b_len);
  ZR_ASSERT_MEMEQ(a_bytes, b_bytes, a_len);
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

ZR_TEST_UNIT(engine_submit_drawlist_invalid_blit_rect_has_no_partial_effects) {
  uint8_t a_bytes[4096];
  uint8_t b_bytes[4096];
  size_t a_len = 0u;
  size_t b_len = 0u;
  uint8_t bad_blit[256];
  const size_t bad_blit_len = zr_make_dl_invalid_blit_rect(bad_blit, sizeof(bad_blit));
  ZR_ASSERT_TRUE(bad_blit_len != 0u);

  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e1 = NULL;
  ZR_ASSERT_TRUE(engine_create(&e1, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e1 != NULL);
  ZR_ASSERT_TRUE(zr_capture_present_bytes(e1, a_bytes, sizeof(a_bytes), &a_len) != 0u);
  engine_destroy(e1);

  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_t* e2 = NULL;
  ZR_ASSERT_TRUE(engine_create(&e2, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e2 != NULL);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e2, bad_blit, (int)bad_blit_len) == ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_TRUE(zr_capture_present_bytes(e2, b_bytes, sizeof(b_bytes), &b_len) != 0u);
  engine_destroy(e2);

  ZR_ASSERT_TRUE(a_len == b_len);
  ZR_ASSERT_MEMEQ(a_bytes, b_bytes, a_len);
}

ZR_TEST_UNIT(engine_submit_drawlist_def_blob_draw_text_run_fixture_executes) {
  uint8_t out[4096];
  size_t out_len = 0u;

  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e, zr_test_dl_fixture3, (int)zr_test_dl_fixture3_len) == ZR_OK);
  ZR_ASSERT_TRUE(zr_capture_present_bytes(e, out, sizeof(out), &out_len) != 0u);
  engine_destroy(e);
}

ZR_TEST_UNIT(engine_submit_drawlist_free_string_invalidates_future_refs) {
  uint8_t def_dl[256];
  uint8_t free_dl[256];
  uint8_t draw_dl[256];

  const size_t def_len = zr_make_dl_def_string(def_dl, sizeof(def_dl), 7u, (const uint8_t*)"ABCD", 4u);
  const size_t free_len = zr_make_dl_free_string(free_dl, sizeof(free_dl), 7u);
  const size_t draw_len = zr_make_dl_draw_text(draw_dl, sizeof(draw_dl), 7u, 4u);
  ZR_ASSERT_TRUE(def_len != 0u);
  ZR_ASSERT_TRUE(free_len != 0u);
  ZR_ASSERT_TRUE(draw_len != 0u);

  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(engine_submit_drawlist(e, def_dl, (int)def_len) == ZR_OK);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e, free_dl, (int)free_len) == ZR_OK);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e, draw_dl, (int)draw_len) != ZR_OK);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_submit_drawlist_overwrite_string_uses_latest_bytes) {
  uint8_t def_old[256];
  uint8_t def_new[256];
  uint8_t draw_dl[256];
  uint8_t out[4096];
  size_t out_len = 0u;

  const size_t old_len = zr_make_dl_def_string(def_old, sizeof(def_old), 9u, (const uint8_t*)"ABCD", 4u);
  const size_t new_len = zr_make_dl_def_string(def_new, sizeof(def_new), 9u, (const uint8_t*)"WXYZ", 4u);
  const size_t draw_len = zr_make_dl_draw_text(draw_dl, sizeof(draw_dl), 9u, 4u);
  ZR_ASSERT_TRUE(old_len != 0u);
  ZR_ASSERT_TRUE(new_len != 0u);
  ZR_ASSERT_TRUE(draw_len != 0u);

  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(engine_submit_drawlist(e, def_old, (int)old_len) == ZR_OK);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e, def_new, (int)new_len) == ZR_OK);
  ZR_ASSERT_TRUE(engine_submit_drawlist(e, draw_dl, (int)draw_len) == ZR_OK);
  ZR_ASSERT_TRUE(zr_capture_present_bytes(e, out, sizeof(out), &out_len) != 0u);
  ZR_ASSERT_TRUE(zr_contains_subseq(out, out_len, (const uint8_t*)"WXYZ", 4u) != 0u);

  engine_destroy(e);
}
