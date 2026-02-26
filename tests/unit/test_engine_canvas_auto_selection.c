/*
  tests/unit/test_engine_canvas_auto_selection.c — Engine AUTO blitter mode selection wiring.

  Why: Ensures engine_submit_drawlist propagates platform pipe/dumb mode into
  blitter AUTO selection so DRAW_CANVAS falls back to ASCII in non-terminal
  contexts even when Unicode capability overrides are forced on.
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"
#include "core/zr_version.h"
#include "zr/zr_drawlist.h"

#include "unit/mock_platform.h"

#include <limits.h>
#include <string.h>

enum {
  ZR_TEST_CANVAS_DL_BYTES_CAP = 256u,
  ZR_TEST_PRESENT_CAPTURE_CAP = 4096u,
  ZR_TEST_DL_MAGIC = 0x4C44525Au,
  ZR_TEST_DL_HEADER_BYTES = 64u,
  ZR_TEST_DL_CMD_HEADER_BYTES = 8u,
  ZR_TEST_DL_DEF_RESOURCE_META_BYTES = 8u,
  ZR_TEST_DL_DRAW_CANVAS_BYTES = 32u,
  ZR_TEST_DL_CMD_COUNT = 3u,
  ZR_TEST_DL_RESERVED_HEADER_WORDS = 9u,
};

static void zr_w16(uint8_t* p, size_t* at, uint16_t v) {
  p[(*at)++] = (uint8_t)(v & 0xFFu);
  p[(*at)++] = (uint8_t)((v >> 8u) & 0xFFu);
}

static void zr_w32(uint8_t* p, size_t* at, uint32_t v) {
  p[(*at)++] = (uint8_t)(v & 0xFFu);
  p[(*at)++] = (uint8_t)((v >> 8u) & 0xFFu);
  p[(*at)++] = (uint8_t)((v >> 16u) & 0xFFu);
  p[(*at)++] = (uint8_t)((v >> 24u) & 0xFFu);
}

static void zr_cmd_header(uint8_t* p, size_t* at, uint16_t opcode, uint32_t size) {
  zr_w16(p, at, opcode);
  zr_w16(p, at, 0u);
  zr_w32(p, at, size);
}

static uint32_t zr_align4_u32(uint32_t n) {
  return (n + 3u) & ~3u;
}

static size_t zr_make_canvas_drawlist_auto(uint8_t* out, size_t out_cap, const uint8_t* blob, uint32_t blob_len) {
  if (!out || (!blob && blob_len != 0u)) {
    return 0u;
  }
  const uint32_t blob_padded = zr_align4_u32(blob_len);
  const uint32_t def_blob_size = ZR_TEST_DL_CMD_HEADER_BYTES + ZR_TEST_DL_DEF_RESOURCE_META_BYTES + blob_padded;
  const uint32_t cmd_bytes = ZR_TEST_DL_CMD_HEADER_BYTES + def_blob_size + ZR_TEST_DL_DRAW_CANVAS_BYTES;
  const uint32_t cmd_count = ZR_TEST_DL_CMD_COUNT;
  const uint32_t total = ZR_TEST_DL_HEADER_BYTES + cmd_bytes;
  size_t at = 0u;
  if ((size_t)total > out_cap) {
    return 0u;
  }

  /*
    Drawlist byte layout used by this fixture:
      [header v1:64][command stream]
    Commands:
      1) CLEAR
      2) DEF_BLOB(id=1)
      3) DRAW_CANVAS (blitter=AUTO) referencing blob id 1
  */
  memset(out, 0, (size_t)total);

  zr_w32(out, &at, ZR_TEST_DL_MAGIC);
  zr_w32(out, &at, ZR_DRAWLIST_VERSION_V1);
  zr_w32(out, &at, ZR_TEST_DL_HEADER_BYTES);
  zr_w32(out, &at, total);
  zr_w32(out, &at, ZR_TEST_DL_HEADER_BYTES);
  zr_w32(out, &at, cmd_bytes);
  zr_w32(out, &at, cmd_count);
  for (uint32_t i = 0u; i < ZR_TEST_DL_RESERVED_HEADER_WORDS; i++) {
    zr_w32(out, &at, 0u);
  }

  zr_cmd_header(out, &at, ZR_DL_OP_CLEAR, ZR_TEST_DL_CMD_HEADER_BYTES);
  zr_cmd_header(out, &at, ZR_DL_OP_DEF_BLOB, def_blob_size);
  zr_w32(out, &at, 1u);
  zr_w32(out, &at, blob_len);
  memcpy(out + at, blob, blob_len);
  at += blob_len;
  for (uint32_t i = blob_len; i < blob_padded; i++) {
    out[at++] = 0u;
  }

  zr_cmd_header(out, &at, ZR_DL_OP_DRAW_CANVAS, ZR_TEST_DL_DRAW_CANVAS_BYTES);
  zr_w16(out, &at, 0u);
  zr_w16(out, &at, 0u);
  zr_w16(out, &at, 1u);
  zr_w16(out, &at, 1u);
  zr_w16(out, &at, 2u);
  zr_w16(out, &at, 2u);
  zr_w32(out, &at, 1u);
  zr_w32(out, &at, 0u);
  out[at++] = (uint8_t)ZR_BLIT_AUTO;
  out[at++] = 0u;
  zr_w16(out, &at, 0u);

  return at;
}

static zr_result_t zr_submit_present_capture(zr_engine_t* e, const uint8_t* dl, size_t dl_len, uint8_t* out,
                                             size_t out_cap, size_t* out_len) {
  if (!e || !dl || !out_len || (!out && out_cap != 0u)) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (dl_len > (size_t)INT_MAX) {
    return ZR_ERR_LIMIT;
  }

  *out_len = 0u;
  mock_plat_clear_writes();

  zr_result_t rc = engine_submit_drawlist(e, dl, (int)dl_len);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = engine_present(e);
  if (rc != ZR_OK) {
    return rc;
  }

  *out_len = mock_plat_last_write_copy(out, out_cap);
  return ZR_OK;
}

static uint8_t zr_has_non_ascii(const uint8_t* bytes, size_t len) {
  if (!bytes) {
    return 0u;
  }
  for (size_t i = 0u; i < len; i++) {
    if (bytes[i] > 0x7Fu) {
      return 1u;
    }
  }
  return 0u;
}

static zr_result_t zr_engine_canvas_auto_setup(zr_engine_t** out_engine) {
  if (!out_engine) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.requested_drawlist_version = ZR_DRAWLIST_VERSION_V1;
  cfg.cap_force_flags = ZR_TERM_CAP_GRAPHEME_CLUSTERS;
  return engine_create(out_engine, &cfg);
}

ZR_TEST_UNIT(engine_canvas_auto_uses_ascii_in_pipe_mode_even_with_unicode_override) {
  static const uint8_t kBlob[16] = {
      255u, 255u, 255u, 255u, 0u, 0u, 0u, 255u, 0u, 0u, 0u, 255u, 255u, 255u, 255u, 255u,
  };

  uint8_t drawlist[ZR_TEST_CANVAS_DL_BYTES_CAP];
  uint8_t present[ZR_TEST_PRESENT_CAPTURE_CAP];
  size_t present_len = 0u;

  mock_plat_reset();
  mock_plat_set_size(1u, 1u);
  mock_plat_set_terminal_query_support(0u);
  mock_plat_set_dumb_terminal(0u);

  const size_t dl_len = zr_make_canvas_drawlist_auto(drawlist, sizeof(drawlist), kBlob, (uint32_t)sizeof(kBlob));
  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(zr_engine_canvas_auto_setup(&e) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(zr_submit_present_capture(e, drawlist, dl_len, present, sizeof(present), &present_len) == ZR_OK);
  ZR_ASSERT_TRUE(present_len != 0u);
  ZR_ASSERT_EQ_U32(zr_has_non_ascii(present, present_len), 0u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_canvas_auto_uses_ascii_in_dumb_mode_even_with_unicode_override) {
  static const uint8_t kBlob[16] = {
      255u, 255u, 255u, 255u, 0u, 0u, 0u, 255u, 0u, 0u, 0u, 255u, 255u, 255u, 255u, 255u,
  };

  uint8_t drawlist[ZR_TEST_CANVAS_DL_BYTES_CAP];
  uint8_t present[ZR_TEST_PRESENT_CAPTURE_CAP];
  size_t present_len = 0u;

  mock_plat_reset();
  mock_plat_set_size(1u, 1u);
  mock_plat_set_terminal_query_support(1u);
  mock_plat_set_dumb_terminal(1u);

  const size_t dl_len = zr_make_canvas_drawlist_auto(drawlist, sizeof(drawlist), kBlob, (uint32_t)sizeof(kBlob));
  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(zr_engine_canvas_auto_setup(&e) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(zr_submit_present_capture(e, drawlist, dl_len, present, sizeof(present), &present_len) == ZR_OK);
  ZR_ASSERT_TRUE(present_len != 0u);
  ZR_ASSERT_EQ_U32(zr_has_non_ascii(present, present_len), 0u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_canvas_auto_uses_unicode_when_terminal_mode_allows_it) {
  static const uint8_t kBlob[16] = {
      255u, 255u, 255u, 255u, 0u, 0u, 0u, 255u, 0u, 0u, 0u, 255u, 255u, 255u, 255u, 255u,
  };

  uint8_t drawlist[ZR_TEST_CANVAS_DL_BYTES_CAP];
  uint8_t present[ZR_TEST_PRESENT_CAPTURE_CAP];
  size_t present_len = 0u;

  mock_plat_reset();
  mock_plat_set_size(1u, 1u);
  mock_plat_set_terminal_query_support(1u);
  mock_plat_set_dumb_terminal(0u);

  const size_t dl_len = zr_make_canvas_drawlist_auto(drawlist, sizeof(drawlist), kBlob, (uint32_t)sizeof(kBlob));
  zr_engine_t* e = NULL;
  ZR_ASSERT_TRUE(zr_engine_canvas_auto_setup(&e) == ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_TRUE(zr_submit_present_capture(e, drawlist, dl_len, present, sizeof(present), &present_len) == ZR_OK);
  ZR_ASSERT_TRUE(present_len != 0u);
  ZR_ASSERT_EQ_U32(zr_has_non_ascii(present, present_len), 1u);

  engine_destroy(e);
}
