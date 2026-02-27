/*
  tests/unit/test_engine_present_single_flush.c — Engine present single-flush contract.

  Why: Validates that engine_present emits terminal bytes via exactly one
  plat_write_output call on success, and does not flush at all when diff output
  cannot fit in the engine-owned per-frame output buffer.
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"

#include "zr/zr_drawlist.h"
#include "zr/zr_version.h"

#include "unit/mock_platform.h"

#include <stdbool.h>
#include <string.h>

extern const uint8_t zr_test_dl_fixture1[];
extern const size_t zr_test_dl_fixture1_len;

enum {
  ZR_TEST_DL_HEADER_BYTES = 64u,
  ZR_TEST_DL_CMD_CLEAR_BYTES = 8u,
  ZR_TEST_DL_CMD_SET_CURSOR_BYTES = 20u,
  ZR_TEST_DL_CMD_DRAW_IMAGE_BYTES = 40u,
  ZR_TEST_IMAGE_BLOB_BYTES = 4u,
  ZR_TEST_IMAGE_PROTOCOL_KITTY = 1u,
};

static const uint8_t ZR_TEST_IMAGE_BLOB[ZR_TEST_IMAGE_BLOB_BYTES] = {0xFFu, 0x00u, 0x00u, 0xFFu};

static bool zr_contains_bytes(const uint8_t* hay, size_t hay_len, const uint8_t* needle, size_t needle_len) {
  if (!hay || !needle || needle_len == 0u || needle_len > hay_len) {
    return false;
  }
  for (size_t i = 0u; i + needle_len <= hay_len; i++) {
    if (memcmp(hay + i, needle, needle_len) == 0) {
      return true;
    }
  }
  return false;
}

static void zr_test_write_u16le(uint8_t* out, size_t* at, uint16_t v) {
  out[(*at)++] = (uint8_t)(v & 0xFFu);
  out[(*at)++] = (uint8_t)((v >> 8u) & 0xFFu);
}

static void zr_test_write_u32le(uint8_t* out, size_t* at, uint32_t v) {
  out[(*at)++] = (uint8_t)(v & 0xFFu);
  out[(*at)++] = (uint8_t)((v >> 8u) & 0xFFu);
  out[(*at)++] = (uint8_t)((v >> 16u) & 0xFFu);
  out[(*at)++] = (uint8_t)((v >> 24u) & 0xFFu);
}

static void zr_test_write_cmd_header(uint8_t* out, size_t* at, uint16_t opcode, uint32_t size) {
  zr_test_write_u16le(out, at, opcode);
  zr_test_write_u16le(out, at, 0u);
  zr_test_write_u32le(out, at, size);
}

static uint32_t zr_test_align4_u32(uint32_t n) {
  return (n + 3u) & ~3u;
}

static void zr_test_write_cursor_image_header(uint8_t* out, size_t* at, uint32_t cmd_bytes, uint32_t total_size) {
  zr_test_write_u32le(out, at, 0x4C44525Au);
  zr_test_write_u32le(out, at, ZR_DRAWLIST_VERSION_V1);
  zr_test_write_u32le(out, at, ZR_TEST_DL_HEADER_BYTES);
  zr_test_write_u32le(out, at, total_size);

  zr_test_write_u32le(out, at, ZR_TEST_DL_HEADER_BYTES);
  zr_test_write_u32le(out, at, cmd_bytes);
  zr_test_write_u32le(out, at, 4u);

  zr_test_write_u32le(out, at, 0u);
  zr_test_write_u32le(out, at, 0u);
  zr_test_write_u32le(out, at, 0u);
  zr_test_write_u32le(out, at, 0u);

  zr_test_write_u32le(out, at, 0u);
  zr_test_write_u32le(out, at, 0u);
  zr_test_write_u32le(out, at, 0u);
  zr_test_write_u32le(out, at, 0u);
  zr_test_write_u32le(out, at, 0u);
}

static void zr_test_write_cursor_image_commands(uint8_t* out, size_t* at) {
  const uint32_t blob_padded = zr_test_align4_u32(ZR_TEST_IMAGE_BLOB_BYTES);
  const uint32_t def_blob_size = 8u + 8u + blob_padded;

  zr_test_write_cmd_header(out, at, ZR_DL_OP_CLEAR, ZR_TEST_DL_CMD_CLEAR_BYTES);
  zr_test_write_cmd_header(out, at, ZR_DL_OP_DEF_BLOB, def_blob_size);
  zr_test_write_u32le(out, at, 1u);
  zr_test_write_u32le(out, at, ZR_TEST_IMAGE_BLOB_BYTES);
  memcpy(out + *at, ZR_TEST_IMAGE_BLOB, sizeof(ZR_TEST_IMAGE_BLOB));
  *at += sizeof(ZR_TEST_IMAGE_BLOB);
  for (uint32_t i = ZR_TEST_IMAGE_BLOB_BYTES; i < blob_padded; i++) {
    out[(*at)++] = 0u;
  }

  zr_test_write_cmd_header(out, at, ZR_DL_OP_SET_CURSOR, ZR_TEST_DL_CMD_SET_CURSOR_BYTES);
  zr_test_write_u32le(out, at, 2u); /* x */
  zr_test_write_u32le(out, at, 1u); /* y */
  out[(*at)++] = 0u;                /* shape=block */
  out[(*at)++] = 1u;                /* visible */
  out[(*at)++] = 0u;                /* blink */
  out[(*at)++] = 0u;                /* reserved */

  zr_test_write_cmd_header(out, at, ZR_DL_OP_DRAW_IMAGE, ZR_TEST_DL_CMD_DRAW_IMAGE_BYTES);
  zr_test_write_u16le(out, at, 0u);            /* dst_col */
  zr_test_write_u16le(out, at, 0u);            /* dst_row */
  zr_test_write_u16le(out, at, 1u);            /* dst_cols */
  zr_test_write_u16le(out, at, 1u);            /* dst_rows */
  zr_test_write_u16le(out, at, 1u);            /* px_width */
  zr_test_write_u16le(out, at, 1u);            /* px_height */
  zr_test_write_u32le(out, at, 1u);            /* blob_id */
  zr_test_write_u32le(out, at, 0u);            /* reserved_blob */
  zr_test_write_u32le(out, at, 7u);            /* image_id */
  out[(*at)++] = 0u;                           /* format=RGBA */
  out[(*at)++] = ZR_TEST_IMAGE_PROTOCOL_KITTY; /* protocol=kitty */
  out[(*at)++] = 0u;                           /* z_layer=0 */
  out[(*at)++] = 0u;                           /* fit=fill */
  out[(*at)++] = 0u;                           /* flags */
  out[(*at)++] = 0u;                           /* reserved0 */
  zr_test_write_u16le(out, at, 0u);            /* reserved1 */
}

/*
  Build a minimal v1 drawlist with CLEAR + DEF_BLOB + SET_CURSOR + DRAW_IMAGE.

  Why: The regression exercises present-path cursor restoration after image
  sideband emission without relying on external fixture generation.
*/
static size_t zr_test_make_cursor_image_drawlist(uint8_t* out, size_t out_cap) {
  const uint32_t blob_padded = zr_test_align4_u32(ZR_TEST_IMAGE_BLOB_BYTES);
  const uint32_t def_blob_size = 8u + 8u + blob_padded;
  const uint32_t cmd_bytes =
      ZR_TEST_DL_CMD_CLEAR_BYTES + def_blob_size + ZR_TEST_DL_CMD_SET_CURSOR_BYTES + ZR_TEST_DL_CMD_DRAW_IMAGE_BYTES;
  const uint32_t total_size = ZR_TEST_DL_HEADER_BYTES + cmd_bytes;
  size_t at = 0u;

  if (!out || out_cap < (size_t)total_size) {
    return 0u;
  }
  memset(out, 0, (size_t)total_size);

  zr_test_write_cursor_image_header(out, &at, cmd_bytes, total_size);
  zr_test_write_cursor_image_commands(out, &at);

  return at;
}

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

ZR_TEST_UNIT(engine_present_restores_cursor_after_image_sideband) {
  uint8_t drawlist_bytes[192];
  uint8_t out[8192];
  static const uint8_t expected_suffix[] = "\x1b[2;3H";
  const size_t dl_len = zr_test_make_cursor_image_drawlist(drawlist_bytes, sizeof(drawlist_bytes));

  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.requested_drawlist_version = ZR_DRAWLIST_VERSION_V1;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  ZR_ASSERT_TRUE(dl_len != 0u);

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(engine_submit_drawlist(e, drawlist_bytes, (int)dl_len) == ZR_OK);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_OK);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);

  const size_t out_len = mock_plat_last_write_copy(out, sizeof(out));
  ZR_ASSERT_TRUE(out_len >= (sizeof(expected_suffix) - 1u));
  ZR_ASSERT_TRUE(
      memcmp(out + out_len - (sizeof(expected_suffix) - 1u), expected_suffix, sizeof(expected_suffix) - 1u) == 0);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_emits_debug_overlay_when_enabled) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.enable_debug_overlay = 1u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_OK);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);

  uint8_t out[8192];
  const size_t out_len = mock_plat_last_write_copy(out, sizeof(out));
  static const uint8_t needle[] = "FPS:";
  ZR_ASSERT_TRUE(zr_contains_bytes(out, out_len, needle, sizeof(needle) - 1u));

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_does_not_emit_debug_overlay_when_disabled) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.enable_debug_overlay = 0u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_OK);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);

  uint8_t out[8192];
  const size_t out_len = mock_plat_last_write_copy(out, sizeof(out));
  static const uint8_t needle[] = "FPS:";
  ZR_ASSERT_TRUE(!zr_contains_bytes(out, out_len, needle, sizeof(needle) - 1u));

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_sync_update_overhead_does_not_force_limit) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_mouse = 0u;
  caps.supports_bracketed_paste = 1u;
  caps.supports_focus_events = 1u;
  caps.supports_osc52 = 0u;
  caps.supports_sync_update = 1u;
  caps.supports_scroll_region = 1u;
  caps.supports_cursor_shape = 0u;
  caps.supports_output_wait_writable = 0u;
  caps.supports_underline_styles = 0u;
  caps.supports_colored_underlines = 0u;
  caps.supports_hyperlinks = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;
  mock_plat_set_caps(caps);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 8u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  mock_plat_clear_writes();
  {
    const zr_result_t rc = engine_present(e);
    if (rc != ZR_OK) {
      zr_test_failf(ctx, __FILE__, __LINE__, "engine_present(e) failed: rc=%d", (int)rc);
      return;
    }
  }
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_present_wraps_output_with_sync_update_when_supported) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_mouse = 0u;
  caps.supports_bracketed_paste = 1u;
  caps.supports_focus_events = 1u;
  caps.supports_osc52 = 0u;
  caps.supports_sync_update = 1u;
  caps.supports_scroll_region = 1u;
  caps.supports_cursor_shape = 0u;
  caps.supports_output_wait_writable = 0u;
  caps.supports_underline_styles = 0u;
  caps.supports_colored_underlines = 0u;
  caps.supports_hyperlinks = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;
  mock_plat_set_caps(caps);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(engine_create(&e, &cfg) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(engine_submit_drawlist(e, zr_test_dl_fixture1, (int)zr_test_dl_fixture1_len) == ZR_OK);

  mock_plat_clear_writes();
  ZR_ASSERT_TRUE(engine_present(e) == ZR_OK);
  ZR_ASSERT_EQ_U32(mock_plat_write_call_count(), 1u);

  static const uint8_t sync_begin[] = "\x1b[?2026h";
  static const uint8_t sync_end[] = "\x1b[?2026l";

  uint8_t out[8192];
  const size_t out_len = mock_plat_last_write_copy(out, sizeof(out));
  ZR_ASSERT_TRUE(out_len >= (sizeof(sync_begin) - 1u) + (sizeof(sync_end) - 1u));

  ZR_ASSERT_TRUE(memcmp(out, sync_begin, sizeof(sync_begin) - 1u) == 0);
  ZR_ASSERT_TRUE(memcmp(out + out_len - (sizeof(sync_end) - 1u), sync_end, sizeof(sync_end) - 1u) == 0);

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
