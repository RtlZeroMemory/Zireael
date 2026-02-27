/*
  tests/unit/test_drawlist_image.c — Unit tests for drawlist v1 DRAW_IMAGE.

  Why: DRAW_IMAGE has protocol/fallback branches and persistent-blob resolution;
  these
  tests pin validation and execute-time behavior.
*/

#include "zr_test.h"

#include "core/zr_drawlist.h"
#include "core/zr_framebuffer.h"
#include "core/zr_image.h"
#include "unicode/zr_width.h"
#include "zr/zr_version.h"

#include <stddef.h>
#include <stdint.h>
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

static size_t zr_make_draw_image_drawlist(uint8_t* out, uint32_t version, const zr_dl_cmd_draw_image_t* cmd,
                                          const uint8_t* blob, uint32_t blob_len) {
  const uint32_t blob_padded = zr_align4_u32(blob_len);
  const uint32_t def_blob_size = 8u + 8u + blob_padded;
  const uint32_t cmd_bytes = 8u + def_blob_size + 40u;
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
  zr_cmd_header(out, &at, ZR_DL_OP_DEF_BLOB, def_blob_size);
  zr_w32(out, &at, 1u);
  zr_w32(out, &at, blob_len);
  memcpy(out + at, blob, blob_len);
  at += blob_len;
  for (uint32_t i = blob_len; i < blob_padded; i++) {
    out[at++] = 0u;
  }
  zr_cmd_header(out, &at, ZR_DL_OP_DRAW_IMAGE, 40u);

  zr_w16(out, &at, cmd->dst_col);
  zr_w16(out, &at, cmd->dst_row);
  zr_w16(out, &at, cmd->dst_cols);
  zr_w16(out, &at, cmd->dst_rows);
  zr_w16(out, &at, cmd->px_width);
  zr_w16(out, &at, cmd->px_height);
  zr_w32(out, &at, cmd->blob_id);
  zr_w32(out, &at, cmd->reserved_blob);
  zr_w32(out, &at, cmd->image_id);
  out[at++] = cmd->format;
  out[at++] = cmd->protocol;
  out[at++] = (uint8_t)cmd->z_layer;
  out[at++] = cmd->fit_mode;
  out[at++] = cmd->flags;
  out[at++] = cmd->reserved0;
  zr_w16(out, &at, cmd->reserved1);

  return at;
}

static zr_result_t zr_validate_draw_image(const uint8_t* bytes, size_t len, zr_dl_view_t* out_view) {
  zr_limits_t lim = zr_limits_default();
  return zr_dl_validate(bytes, len, &lim, out_view);
}

ZR_TEST_UNIT(drawlist_image_validate_v6_accepts_basic_rgba) {
  uint8_t blob[4] = {9u, 8u, 7u, 255u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_image_t cmd;
  zr_dl_view_t view;

  memset(&cmd, 0, sizeof(cmd));
  cmd.dst_col = 0u;
  cmd.dst_row = 0u;
  cmd.dst_cols = 1u;
  cmd.dst_rows = 1u;
  cmd.px_width = 1u;
  cmd.px_height = 1u;
  cmd.blob_id = 1u;
  cmd.image_id = 1u;
  cmd.format = (uint8_t)ZR_IMAGE_FORMAT_RGBA;
  cmd.protocol = 0u;
  cmd.fit_mode = (uint8_t)ZR_IMAGE_FIT_FILL;

  const size_t len = zr_make_draw_image_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u);
  ZR_ASSERT_EQ_U32(zr_validate_draw_image(bytes, len, &view), ZR_OK);
}

ZR_TEST_UNIT(drawlist_image_validate_version_above_v1_rejects_protocol_version) {
  uint8_t blob[4] = {9u, 8u, 7u, 255u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_image_t cmd;
  zr_dl_view_t view;

  memset(&cmd, 0, sizeof(cmd));
  cmd.dst_cols = 1u;
  cmd.dst_rows = 1u;
  cmd.px_width = 1u;
  cmd.px_height = 1u;
  cmd.blob_id = 1u;
  cmd.image_id = 1u;
  cmd.format = (uint8_t)ZR_IMAGE_FORMAT_RGBA;
  cmd.fit_mode = (uint8_t)ZR_IMAGE_FIT_FILL;

  const size_t len = zr_make_draw_image_drawlist(bytes, 4u, &cmd, blob, 4u);
  ZR_ASSERT_EQ_U32(zr_validate_draw_image(bytes, len, &view), ZR_ERR_UNSUPPORTED);
}

ZR_TEST_UNIT(drawlist_image_validate_rejects_invalid_fit_mode) {
  uint8_t blob[4] = {9u, 8u, 7u, 255u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_image_t cmd;
  zr_dl_view_t view;

  memset(&cmd, 0, sizeof(cmd));
  cmd.dst_cols = 1u;
  cmd.dst_rows = 1u;
  cmd.px_width = 1u;
  cmd.px_height = 1u;
  cmd.blob_id = 1u;
  cmd.image_id = 1u;
  cmd.format = (uint8_t)ZR_IMAGE_FORMAT_RGBA;
  cmd.fit_mode = 9u;

  const size_t len = zr_make_draw_image_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u);
  ZR_ASSERT_EQ_U32(zr_validate_draw_image(bytes, len, &view), ZR_ERR_FORMAT);
}

ZR_TEST_UNIT(drawlist_image_execute_fallback_rgba_when_no_protocol) {
  uint8_t blob[4] = {9u, 8u, 7u, 255u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_image_t cmd;
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t view;
  zr_fb_t fb;
  zr_cursor_state_t cursor;
  zr_dl_resources_t resources;

  memset(&cmd, 0, sizeof(cmd));
  cmd.dst_col = 0u;
  cmd.dst_row = 0u;
  cmd.dst_cols = 1u;
  cmd.dst_rows = 1u;
  cmd.px_width = 1u;
  cmd.px_height = 1u;
  cmd.blob_id = 1u;
  cmd.image_id = 3u;
  cmd.format = (uint8_t)ZR_IMAGE_FORMAT_RGBA;
  cmd.protocol = 0u;
  cmd.fit_mode = (uint8_t)ZR_IMAGE_FIT_FILL;

  const size_t len = zr_make_draw_image_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u);

  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes, len, &lim, &view), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  memset(&cursor, 0, sizeof(cursor));
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  zr_dl_resources_init(&resources);

  ZR_ASSERT_EQ_U32(
      zr_dl_execute(&view, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, NULL, NULL, &resources, &cursor), ZR_OK);

  {
    const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
    ZR_ASSERT_TRUE(c != NULL);
    ZR_ASSERT_EQ_U32(c->style.bg_rgb, 0x00090807u);
  }

  zr_dl_resources_release(&resources);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_image_execute_png_without_protocol_is_unsupported) {
  uint8_t blob[4] = {0x89u, 0x50u, 0x4Eu, 0x47u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_image_t cmd;
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t view;
  zr_fb_t fb;
  zr_cursor_state_t cursor;
  zr_dl_resources_t resources;

  memset(&cmd, 0, sizeof(cmd));
  cmd.dst_col = 0u;
  cmd.dst_row = 0u;
  cmd.dst_cols = 1u;
  cmd.dst_rows = 1u;
  cmd.px_width = 1u;
  cmd.px_height = 1u;
  cmd.blob_id = 1u;
  cmd.image_id = 4u;
  cmd.format = (uint8_t)ZR_IMAGE_FORMAT_PNG;
  cmd.protocol = 0u;
  cmd.fit_mode = (uint8_t)ZR_IMAGE_FIT_FILL;

  const size_t len = zr_make_draw_image_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u);

  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes, len, &lim, &view), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  memset(&cursor, 0, sizeof(cursor));
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  zr_dl_resources_init(&resources);

  ZR_ASSERT_EQ_U32(
      zr_dl_execute(&view, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, NULL, NULL, &resources, &cursor),
      ZR_ERR_UNSUPPORTED);

  zr_dl_resources_release(&resources);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_image_execute_with_kitty_profile_stages_frame) {
  uint8_t blob[4] = {1u, 2u, 3u, 255u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_image_t cmd;
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t view;
  zr_fb_t fb;
  zr_cursor_state_t cursor;
  zr_dl_resources_t resources;
  zr_image_frame_t stage;
  zr_terminal_profile_t profile;

  memset(&cmd, 0, sizeof(cmd));
  cmd.dst_col = 0u;
  cmd.dst_row = 0u;
  cmd.dst_cols = 1u;
  cmd.dst_rows = 1u;
  cmd.px_width = 1u;
  cmd.px_height = 1u;
  cmd.blob_id = 1u;
  cmd.image_id = 5u;
  cmd.format = (uint8_t)ZR_IMAGE_FORMAT_RGBA;
  cmd.protocol = 0u;
  cmd.fit_mode = (uint8_t)ZR_IMAGE_FIT_FILL;

  memset(&profile, 0, sizeof(profile));
  profile.supports_kitty_graphics = 1u;

  const size_t len = zr_make_draw_image_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u);

  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes, len, &lim, &view), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  memset(&cursor, 0, sizeof(cursor));
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  zr_dl_resources_init(&resources);

  zr_image_frame_init(&stage);

  ZR_ASSERT_EQ_U32(
      zr_dl_execute(&view, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, &profile, &stage, &resources, &cursor),
      ZR_OK);

  ZR_ASSERT_EQ_U32(stage.cmds_len, 1u);
  ZR_ASSERT_EQ_U32(stage.blob_len, 4u);
  ZR_ASSERT_EQ_U32(stage.cmds[0].image_id, 5u);
  ZR_ASSERT_EQ_U32(stage.cmds[0].format, (uint8_t)ZR_IMAGE_FORMAT_RGBA);
  ZR_ASSERT_EQ_U32(stage.cmds[0].protocol, (uint8_t)ZR_IMG_PROTO_KITTY);
  ZR_ASSERT_MEMEQ(stage.blob_bytes, blob, 4u);

  {
    const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
    ZR_ASSERT_TRUE(c != NULL);
    ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
    ZR_ASSERT_EQ_U32(c->glyph[0], (uint8_t)' ');
  }

  zr_image_frame_release(&stage);
  zr_dl_resources_release(&resources);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_image_execute_with_protocol_requires_stage_buffer) {
  uint8_t blob[4] = {1u, 2u, 3u, 255u};
  uint8_t bytes[160];
  zr_dl_cmd_draw_image_t cmd;
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t view;
  zr_fb_t fb;
  zr_cursor_state_t cursor;
  zr_dl_resources_t resources;
  zr_terminal_profile_t profile;

  memset(&cmd, 0, sizeof(cmd));
  cmd.dst_col = 0u;
  cmd.dst_row = 0u;
  cmd.dst_cols = 1u;
  cmd.dst_rows = 1u;
  cmd.px_width = 1u;
  cmd.px_height = 1u;
  cmd.blob_id = 1u;
  cmd.image_id = 6u;
  cmd.format = (uint8_t)ZR_IMAGE_FORMAT_RGBA;
  cmd.protocol = 0u;
  cmd.fit_mode = (uint8_t)ZR_IMAGE_FIT_FILL;

  memset(&profile, 0, sizeof(profile));
  profile.supports_kitty_graphics = 1u;

  const size_t len = zr_make_draw_image_drawlist(bytes, ZR_DRAWLIST_VERSION_V1, &cmd, blob, 4u);

  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes, len, &lim, &view), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 1u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);

  memset(&cursor, 0, sizeof(cursor));
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;
  zr_dl_resources_init(&resources);

  ZR_ASSERT_EQ_U32(
      zr_dl_execute(&view, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, &profile, NULL, &resources, &cursor),
      ZR_ERR_INVALID_ARGUMENT);

  zr_dl_resources_release(&resources);
  zr_fb_release(&fb);
}
