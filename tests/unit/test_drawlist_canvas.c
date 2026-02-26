/*
  tests/unit/test_drawlist_canvas.c — Unit tests for DRAW_CANVAS opcode.

  Why: Validates v1 opcode framing, bounds checks, and framebuffer execution.
*/

#include "zr_test.h"

#include "core/zr_drawlist.h"
#include "core/zr_framebuffer.h"
#include "unicode/zr_width.h"
#include "zr/zr_version.h"

#include <string.h>

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

static size_t zr_make_canvas_drawlist(uint8_t* out, uint32_t version, const zr_dl_cmd_draw_canvas_t* cmd,
                                      const uint8_t* blob, uint32_t blob_len, uint8_t with_clip) {
  const uint32_t blob_padded = zr_align4_u32(blob_len);
  const uint32_t def_blob_size = 8u + 8u + blob_padded;
  const uint32_t cmd_bytes = with_clip != 0u ? (8u + def_blob_size + 24u + 32u + 8u) : (8u + def_blob_size + 32u);
  const uint32_t cmd_count = with_clip != 0u ? 5u : 3u;
  const uint32_t total = 64u + cmd_bytes;
  size_t at = 0u;

  memset(out, 0, (size_t)total);

  zr_w32(out, &at, 0x4C44525Au);
  zr_w32(out, &at, version);
  zr_w32(out, &at, 64u);
  zr_w32(out, &at, total);
  zr_w32(out, &at, 64u);
  zr_w32(out, &at, cmd_bytes);
  zr_w32(out, &at, cmd_count);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);

  zr_cmd_header(out, &at, ZR_DL_OP_CLEAR, 8u);
  zr_cmd_header(out, &at, ZR_DL_OP_DEF_BLOB, def_blob_size);
  zr_w32(out, &at, 1u);
  zr_w32(out, &at, blob_len);
  memcpy(out + at, blob, blob_len);
  at += blob_len;
  for (uint32_t i = blob_len; i < blob_padded; i++) {
    out[at++] = 0u;
  }
  if (with_clip != 0u) {
    zr_cmd_header(out, &at, ZR_DL_OP_PUSH_CLIP, 24u);
    zr_w32(out, &at, 0u);
    zr_w32(out, &at, 0u);
    zr_w32(out, &at, 1u);
    zr_w32(out, &at, 1u);
  }

  zr_cmd_header(out, &at, ZR_DL_OP_DRAW_CANVAS, 32u);
  zr_w16(out, &at, cmd->dst_col);
  zr_w16(out, &at, cmd->dst_row);
  zr_w16(out, &at, cmd->dst_cols);
  zr_w16(out, &at, cmd->dst_rows);
  zr_w16(out, &at, cmd->px_width);
  zr_w16(out, &at, cmd->px_height);
  zr_w32(out, &at, cmd->blob_id);
  zr_w32(out, &at, cmd->reserved0);
  out[at++] = cmd->blitter;
  out[at++] = cmd->flags;
  zr_w16(out, &at, cmd->reserved);

  if (with_clip != 0u) {
    zr_cmd_header(out, &at, ZR_DL_OP_POP_CLIP, 8u);
  }

  return at;
}

static size_t zr_make_canvas_free_drawlist(uint8_t* out, uint32_t version, const zr_dl_cmd_draw_canvas_t* cmd,
                                           uint32_t free_blob_id) {
  const uint32_t cmd_bytes = 8u + 12u + 32u;
  const uint32_t total = 64u + cmd_bytes;
  size_t at = 0u;

  memset(out, 0, (size_t)total);

  zr_w32(out, &at, 0x4C44525Au);
  zr_w32(out, &at, version);
  zr_w32(out, &at, 64u);
  zr_w32(out, &at, total);
  zr_w32(out, &at, 64u);
  zr_w32(out, &at, cmd_bytes);
  zr_w32(out, &at, 3u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);

  zr_cmd_header(out, &at, ZR_DL_OP_CLEAR, 8u);
  zr_cmd_header(out, &at, ZR_DL_OP_FREE_BLOB, 12u);
  zr_w32(out, &at, free_blob_id);
  zr_cmd_header(out, &at, ZR_DL_OP_DRAW_CANVAS, 32u);
  zr_w16(out, &at, cmd->dst_col);
  zr_w16(out, &at, cmd->dst_row);
  zr_w16(out, &at, cmd->dst_cols);
  zr_w16(out, &at, cmd->dst_rows);
  zr_w16(out, &at, cmd->px_width);
  zr_w16(out, &at, cmd->px_height);
  zr_w32(out, &at, cmd->blob_id);
  zr_w32(out, &at, cmd->reserved0);
  out[at++] = cmd->blitter;
  out[at++] = cmd->flags;
  zr_w16(out, &at, cmd->reserved);
  return at;
}

static zr_result_t zr_exec_canvas(const uint8_t* bytes, size_t len, zr_fb_t* fb) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  zr_cursor_state_t cursor = {0};
  zr_dl_resources_t resources;
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  zr_dl_resources_init(&resources);

  zr_result_t rc = zr_dl_validate(bytes, len, &lim, &v);
  if (rc != ZR_OK) {
    zr_dl_resources_release(&resources);
    return rc;
  }
  rc = zr_dl_execute(&v, fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, NULL, NULL, &resources, &cursor);
  zr_dl_resources_release(&resources);
  return rc;
}

ZR_TEST_UNIT(drawlist_canvas_valid_executes_and_writes_cell) {
  uint8_t blob[4] = {12u, 34u, 56u, 255u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_canvas_t cmd = {0, 0, 1, 1, 1, 1, 1u, 0u, (uint8_t)ZR_BLIT_ASCII, 0u, 0u};
  size_t len = zr_make_canvas_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u, 0u);
  zr_fb_t fb;

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_canvas(bytes, len, &fb), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], (uint8_t)' ');
  ZR_ASSERT_EQ_U32(c->style.bg_rgb, 0x000C2238u);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_canvas_bounds_exceeded_is_invalid_argument) {
  uint8_t blob[4] = {1u, 2u, 3u, 255u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_canvas_t cmd = {1, 0, 1, 1, 1, 1, 1u, 0u, (uint8_t)ZR_BLIT_ASCII, 0u, 0u};
  size_t len = zr_make_canvas_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u, 0u);
  zr_fb_t fb;

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_canvas(bytes, len, &fb), ZR_ERR_INVALID_ARGUMENT);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_canvas_missing_blob_rejected) {
  uint8_t blob[4] = {1u, 2u, 3u, 255u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_canvas_t cmd = {0, 0, 1, 1, 1, 1, 2u, 0u, (uint8_t)ZR_BLIT_ASCII, 0u, 0u};
  size_t len = zr_make_canvas_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u, 0u);
  zr_fb_t fb;

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_canvas(bytes, len, &fb), ZR_ERR_FORMAT);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_canvas_blob_len_mismatch_rejected) {
  uint8_t blob[4] = {1u, 2u, 3u, 255u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_canvas_t cmd = {0, 0, 2, 1, 2, 1, 1u, 0u, (uint8_t)ZR_BLIT_ASCII, 0u, 0u};
  size_t len = zr_make_canvas_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u, 0u);
  zr_fb_t fb;

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 2u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_canvas(bytes, len, &fb), ZR_ERR_INVALID_ARGUMENT);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_canvas_overwrite_blob_uses_latest_bytes) {
  uint8_t blob_red[4] = {255u, 0u, 0u, 255u};
  uint8_t blob_blue[4] = {0u, 0u, 255u, 255u};
  uint8_t bytes_red[160];
  uint8_t bytes_blue[160];
  zr_dl_cmd_draw_canvas_t cmd = {0, 0, 1, 1, 1, 1, 1u, 0u, (uint8_t)ZR_BLIT_ASCII, 0u, 0u};
  const size_t len_red = zr_make_canvas_drawlist(bytes_red, ZR_DRAWLIST_VERSION_V1, &cmd, blob_red, 4u, 0u);
  const size_t len_blue = zr_make_canvas_drawlist(bytes_blue, ZR_DRAWLIST_VERSION_V1, &cmd, blob_blue, 4u, 0u);
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  zr_cursor_state_t cursor = {0};
  zr_dl_resources_t resources;
  zr_fb_t fb;

  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  zr_dl_resources_init(&resources);
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes_red, len_red, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(
      zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, NULL, NULL, &resources, &cursor), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes_blue, len_blue, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(
      zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, NULL, NULL, &resources, &cursor), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->style.bg_rgb, 0x000000FFu);

  zr_dl_resources_release(&resources);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_canvas_free_blob_invalidates_future_refs) {
  uint8_t blob[4] = {255u, 0u, 0u, 255u};
  uint8_t bytes_def[160];
  uint8_t bytes_free_draw[160];
  zr_dl_cmd_draw_canvas_t cmd = {0, 0, 1, 1, 1, 1, 1u, 0u, (uint8_t)ZR_BLIT_ASCII, 0u, 0u};
  const size_t len_def = zr_make_canvas_drawlist(bytes_def, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u, 0u);
  const size_t len_free_draw = zr_make_canvas_free_drawlist(bytes_free_draw, ZR_DRAWLIST_VERSION_V1, &cmd, 1u);
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  zr_cursor_state_t cursor = {0};
  zr_dl_resources_t resources;
  zr_fb_t fb;

  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  zr_dl_resources_init(&resources);
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes_def, len_def, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(
      zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, NULL, NULL, &resources, &cursor), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes_free_draw, len_free_draw, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, NULL, NULL, &resources,
                                 &cursor),
                   ZR_ERR_FORMAT);

  zr_dl_resources_release(&resources);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_canvas_invalid_blitter_rejected) {
  uint8_t blob[4] = {1u, 2u, 3u, 255u};
  uint8_t bytes[160];
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  zr_dl_cmd_draw_canvas_t cmd = {0, 0, 1, 1, 1, 1, 1u, 0u, 99u, 0u, 0u};
  size_t len = zr_make_canvas_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u, 0u);

  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes, len, &lim, &v), ZR_ERR_FORMAT);
}

ZR_TEST_UNIT(drawlist_canvas_zero_dimensions_rejected) {
  uint8_t blob[4] = {1u, 2u, 3u, 255u};
  uint8_t bytes[160];
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  zr_dl_cmd_draw_canvas_t cmd = {0, 0, 0, 1, 1, 1, 1u, 0u, (uint8_t)ZR_BLIT_ASCII, 0u, 0u};
  size_t len = zr_make_canvas_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u, 0u);

  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes, len, &lim, &v), ZR_ERR_FORMAT);
}

ZR_TEST_UNIT(drawlist_canvas_versions_above_v1_rejected_as_unsupported) {
  uint8_t blob[4] = {1u, 2u, 3u, 255u};
  uint8_t bytes[160];
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  zr_dl_cmd_draw_canvas_t cmd = {0, 0, 1, 1, 1, 1, 1u, 0u, (uint8_t)ZR_BLIT_ASCII, 0u, 0u};
  size_t len = zr_make_canvas_drawlist(bytes, 2u, &cmd, blob, 4u, 0u);
  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes, len, &lim, &v), ZR_ERR_UNSUPPORTED);
  len = zr_make_canvas_drawlist(bytes, 3u, &cmd, blob, 4u, 0u);
  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes, len, &lim, &v), ZR_ERR_UNSUPPORTED);
}

ZR_TEST_UNIT(drawlist_canvas_respects_clip_rectangle) {
  uint8_t blob[8] = {255u, 0u, 0u, 255u, 0u, 0u, 255u, 255u};
  uint8_t bytes[192];
  zr_dl_cmd_draw_canvas_t cmd = {0, 0, 2, 1, 2, 1, 1u, 0u, (uint8_t)ZR_BLIT_ASCII, 0u, 0u};
  size_t len = zr_make_canvas_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 8u, 1u);
  zr_fb_t fb;

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 2u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_canvas(bytes, len, &fb), ZR_OK);

  const zr_cell_t* c0 = zr_fb_cell_const(&fb, 0u, 0u);
  const zr_cell_t* c1 = zr_fb_cell_const(&fb, 1u, 0u);
  ZR_ASSERT_TRUE(c0 != NULL && c1 != NULL);
  ZR_ASSERT_EQ_U32(c0->style.bg_rgb, 0x00FF0000u);
  ZR_ASSERT_EQ_U32(c1->style.bg_rgb, 0u);
  zr_fb_release(&fb);
}
