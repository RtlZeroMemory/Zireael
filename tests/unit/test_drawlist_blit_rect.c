/*
  tests/unit/test_drawlist_blit_rect.c — Unit tests for drawlist BLIT_RECT opcode.

  Why: Verifies overlap-safe copy semantics and metadata preservation when
  drawlists copy framebuffer cell rectangles.
*/

#include "zr_test.h"

#include "core/zr_drawlist.h"
#include "core/zr_framebuffer.h"
#include "unicode/zr_width.h"
#include "zr/zr_version.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct zr_test_style_wire_t {
  uint32_t fg;
  uint32_t bg;
  uint32_t attrs;
  uint32_t reserved0;
  uint32_t underline_rgb;
  uint32_t link_uri_ref;
  uint32_t link_id_ref;
} zr_test_style_wire_t;

typedef struct zr_dl_builder_t {
  uint8_t* out;
  size_t out_cap;
  size_t at;
  uint32_t cmd_count;
} zr_dl_builder_t;

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

static void zr_wi32(uint8_t* p, size_t* at, int32_t v) {
  zr_w32(p, at, (uint32_t)v);
}

static uint32_t zr_align4_u32(uint32_t n) {
  return (n + 3u) & ~3u;
}

static uint8_t zr_builder_reserve(const zr_dl_builder_t* b, size_t bytes) {
  return b && b->out && b->at <= b->out_cap && bytes <= (b->out_cap - b->at);
}

static void zr_builder_cmd_header(zr_dl_builder_t* b, uint16_t opcode, uint32_t size) {
  zr_w16(b->out, &b->at, opcode);
  zr_w16(b->out, &b->at, 0u);
  zr_w32(b->out, &b->at, size);
  b->cmd_count += 1u;
}

static void zr_builder_init(zr_dl_builder_t* b, uint8_t* out, size_t out_cap) {
  memset(out, 0, out_cap);
  b->out = out;
  b->out_cap = out_cap;
  b->at = 64u;
  b->cmd_count = 0u;
}

static size_t zr_builder_finish(zr_dl_builder_t* b, uint32_t version) {
  size_t h = 0u;
  const uint32_t total = (uint32_t)b->at;
  const uint32_t cmd_bytes = (uint32_t)(b->at - 64u);

  zr_w32(b->out, &h, 0x4C44525Au);
  zr_w32(b->out, &h, version);
  zr_w32(b->out, &h, 64u);
  zr_w32(b->out, &h, total);
  zr_w32(b->out, &h, 64u);
  zr_w32(b->out, &h, cmd_bytes);
  zr_w32(b->out, &h, b->cmd_count);
  zr_w32(b->out, &h, 0u);
  zr_w32(b->out, &h, 0u);
  zr_w32(b->out, &h, 0u);
  zr_w32(b->out, &h, 0u);
  zr_w32(b->out, &h, 0u);
  zr_w32(b->out, &h, 0u);
  zr_w32(b->out, &h, 0u);
  zr_w32(b->out, &h, 0u);
  zr_w32(b->out, &h, 0u);
  return b->at;
}

static uint8_t zr_builder_cmd_clear(zr_dl_builder_t* b) {
  if (!zr_builder_reserve(b, 8u)) {
    return 0u;
  }
  zr_builder_cmd_header(b, ZR_DL_OP_CLEAR, 8u);
  return 1u;
}

static uint8_t zr_builder_cmd_def_string(zr_dl_builder_t* b, uint32_t id, const uint8_t* bytes, uint32_t len) {
  const uint32_t padded = zr_align4_u32(len);
  const uint32_t size = 8u + 8u + padded;
  if (!bytes || !zr_builder_reserve(b, size)) {
    return 0u;
  }
  zr_builder_cmd_header(b, ZR_DL_OP_DEF_STRING, size);
  zr_w32(b->out, &b->at, id);
  zr_w32(b->out, &b->at, len);
  memcpy(b->out + b->at, bytes, len);
  b->at += len;
  for (uint32_t i = len; i < padded; i++) {
    b->out[b->at++] = 0u;
  }
  return 1u;
}

static uint8_t zr_builder_cmd_draw_text(zr_dl_builder_t* b, int32_t x, int32_t y, uint32_t string_id, uint32_t byte_off,
                                        uint32_t byte_len, const zr_test_style_wire_t* style) {
  if (!style || !zr_builder_reserve(b, 60u)) {
    return 0u;
  }
  zr_builder_cmd_header(b, ZR_DL_OP_DRAW_TEXT, 60u);
  zr_wi32(b->out, &b->at, x);
  zr_wi32(b->out, &b->at, y);
  zr_w32(b->out, &b->at, string_id);
  zr_w32(b->out, &b->at, byte_off);
  zr_w32(b->out, &b->at, byte_len);
  zr_w32(b->out, &b->at, style->fg);
  zr_w32(b->out, &b->at, style->bg);
  zr_w32(b->out, &b->at, style->attrs);
  zr_w32(b->out, &b->at, style->reserved0);
  zr_w32(b->out, &b->at, style->underline_rgb);
  zr_w32(b->out, &b->at, style->link_uri_ref);
  zr_w32(b->out, &b->at, style->link_id_ref);
  zr_w32(b->out, &b->at, 0u);
  return 1u;
}

static uint8_t zr_builder_cmd_blit_rect(zr_dl_builder_t* b, int32_t src_x, int32_t src_y, int32_t w, int32_t h,
                                        int32_t dst_x, int32_t dst_y) {
  if (!zr_builder_reserve(b, 32u)) {
    return 0u;
  }
  zr_builder_cmd_header(b, ZR_DL_OP_BLIT_RECT, 32u);
  zr_wi32(b->out, &b->at, src_x);
  zr_wi32(b->out, &b->at, src_y);
  zr_wi32(b->out, &b->at, w);
  zr_wi32(b->out, &b->at, h);
  zr_wi32(b->out, &b->at, dst_x);
  zr_wi32(b->out, &b->at, dst_y);
  return 1u;
}

static zr_test_style_wire_t zr_style_wire_plain(uint32_t fg) {
  zr_test_style_wire_t s;
  memset(&s, 0, sizeof(s));
  s.fg = fg;
  return s;
}

static zr_result_t zr_exec_drawlist(const uint8_t* bytes, size_t len, zr_fb_t* fb) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  zr_cursor_state_t cursor;
  zr_dl_resources_t resources;
  zr_result_t rc = ZR_OK;

  memset(&cursor, 0, sizeof(cursor));
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;

  zr_dl_resources_init(&resources);
  rc = zr_dl_validate(bytes, len, &lim, &v);
  if (rc == ZR_OK) {
    rc = zr_dl_execute(&v, fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, NULL, NULL, &resources, &cursor);
  }
  zr_dl_resources_release(&resources);
  return rc;
}

static void zr_assert_cell_ascii(zr_test_ctx_t* ctx, const zr_fb_t* fb, uint32_t x, uint32_t y, uint8_t glyph,
                                 uint32_t fg) {
  const zr_cell_t* c = zr_fb_cell_const(fb, x, y);
  ZR_ASSERT_TRUE(c != NULL);
  ZR_ASSERT_EQ_U32(c->width, 1u);
  ZR_ASSERT_EQ_U32(c->glyph_len, 1u);
  ZR_ASSERT_EQ_U32(c->glyph[0], glyph);
  ZR_ASSERT_EQ_U32(c->style.fg_rgb, fg);
}

ZR_TEST_UNIT(drawlist_blit_rect_non_overlap_copy) {
  uint8_t bytes[1024];
  zr_dl_builder_t b;
  const uint8_t row0[] = "abc";
  const uint8_t row1[] = "def";
  const zr_test_style_wire_t s0 = zr_style_wire_plain(0x11111111u);
  const zr_test_style_wire_t s1 = zr_style_wire_plain(0x22222222u);
  zr_fb_t fb;

  zr_builder_init(&b, bytes, sizeof(bytes));
  ZR_ASSERT_TRUE(zr_builder_cmd_clear(&b) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 1u, row0, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 2u, row1, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 1, 1, 1u, 0u, 3u, &s0) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 1, 2, 2u, 0u, 3u, &s1) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_blit_rect(&b, 1, 1, 3, 2, 4, 0) != 0u);
  const size_t len = zr_builder_finish(&b, ZR_DRAWLIST_VERSION_V1);

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 8u, 4u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_drawlist(bytes, len, &fb), ZR_OK);

  zr_assert_cell_ascii(ctx, &fb, 4u, 0u, (uint8_t)'a', 0x11111111u);
  zr_assert_cell_ascii(ctx, &fb, 5u, 0u, (uint8_t)'b', 0x11111111u);
  zr_assert_cell_ascii(ctx, &fb, 6u, 0u, (uint8_t)'c', 0x11111111u);
  zr_assert_cell_ascii(ctx, &fb, 4u, 1u, (uint8_t)'d', 0x22222222u);
  zr_assert_cell_ascii(ctx, &fb, 5u, 1u, (uint8_t)'e', 0x22222222u);
  zr_assert_cell_ascii(ctx, &fb, 6u, 1u, (uint8_t)'f', 0x22222222u);

  zr_assert_cell_ascii(ctx, &fb, 1u, 1u, (uint8_t)'a', 0x11111111u);
  zr_assert_cell_ascii(ctx, &fb, 1u, 2u, (uint8_t)'d', 0x22222222u);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_blit_rect_overlap_vertical_scroll_down) {
  uint8_t bytes[1024];
  zr_dl_builder_t b;
  const uint8_t r0[] = "AAA";
  const uint8_t r1[] = "BBB";
  const uint8_t r2[] = "CCC";
  const uint8_t r3[] = "DDD";
  const zr_test_style_wire_t s0 = zr_style_wire_plain(1u);
  const zr_test_style_wire_t s1 = zr_style_wire_plain(2u);
  const zr_test_style_wire_t s2 = zr_style_wire_plain(3u);
  const zr_test_style_wire_t s3 = zr_style_wire_plain(4u);
  zr_fb_t fb;

  zr_builder_init(&b, bytes, sizeof(bytes));
  ZR_ASSERT_TRUE(zr_builder_cmd_clear(&b) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 1u, r0, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 2u, r1, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 3u, r2, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 4u, r3, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 1, 0, 1u, 0u, 3u, &s0) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 1, 1, 2u, 0u, 3u, &s1) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 1, 2, 3u, 0u, 3u, &s2) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 1, 3, 4u, 0u, 3u, &s3) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_blit_rect(&b, 1, 0, 3, 4, 1, 1) != 0u);
  const size_t len = zr_builder_finish(&b, ZR_DRAWLIST_VERSION_V1);

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 5u, 5u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_drawlist(bytes, len, &fb), ZR_OK);

  zr_assert_cell_ascii(ctx, &fb, 1u, 1u, (uint8_t)'A', 1u);
  zr_assert_cell_ascii(ctx, &fb, 1u, 2u, (uint8_t)'B', 2u);
  zr_assert_cell_ascii(ctx, &fb, 1u, 3u, (uint8_t)'C', 3u);
  zr_assert_cell_ascii(ctx, &fb, 1u, 4u, (uint8_t)'D', 4u);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_blit_rect_overlap_vertical_scroll_up) {
  uint8_t bytes[1024];
  zr_dl_builder_t b;
  const uint8_t r0[] = "AAA";
  const uint8_t r1[] = "BBB";
  const uint8_t r2[] = "CCC";
  const uint8_t r3[] = "DDD";
  const zr_test_style_wire_t s0 = zr_style_wire_plain(1u);
  const zr_test_style_wire_t s1 = zr_style_wire_plain(2u);
  const zr_test_style_wire_t s2 = zr_style_wire_plain(3u);
  const zr_test_style_wire_t s3 = zr_style_wire_plain(4u);
  zr_fb_t fb;

  zr_builder_init(&b, bytes, sizeof(bytes));
  ZR_ASSERT_TRUE(zr_builder_cmd_clear(&b) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 1u, r0, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 2u, r1, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 3u, r2, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 4u, r3, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 1, 1, 1u, 0u, 3u, &s0) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 1, 2, 2u, 0u, 3u, &s1) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 1, 3, 3u, 0u, 3u, &s2) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 1, 4, 4u, 0u, 3u, &s3) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_blit_rect(&b, 1, 1, 3, 4, 1, 0) != 0u);
  const size_t len = zr_builder_finish(&b, ZR_DRAWLIST_VERSION_V1);

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 5u, 5u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_drawlist(bytes, len, &fb), ZR_OK);

  zr_assert_cell_ascii(ctx, &fb, 1u, 0u, (uint8_t)'A', 1u);
  zr_assert_cell_ascii(ctx, &fb, 1u, 1u, (uint8_t)'B', 2u);
  zr_assert_cell_ascii(ctx, &fb, 1u, 2u, (uint8_t)'C', 3u);
  zr_assert_cell_ascii(ctx, &fb, 1u, 3u, (uint8_t)'D', 4u);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_blit_rect_overlap_horizontal_shift) {
  uint8_t bytes[512];
  zr_dl_builder_t b;
  const uint8_t row[] = "ABCDE";
  const zr_test_style_wire_t s = zr_style_wire_plain(0xABu);
  zr_fb_t fb;

  zr_builder_init(&b, bytes, sizeof(bytes));
  ZR_ASSERT_TRUE(zr_builder_cmd_clear(&b) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 1u, row, 5u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 0, 0, 1u, 0u, 5u, &s) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_blit_rect(&b, 0, 0, 5, 1, 1, 0) != 0u);
  const size_t len = zr_builder_finish(&b, ZR_DRAWLIST_VERSION_V1);

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 6u, 1u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_drawlist(bytes, len, &fb), ZR_OK);

  zr_assert_cell_ascii(ctx, &fb, 0u, 0u, (uint8_t)'A', 0xABu);
  zr_assert_cell_ascii(ctx, &fb, 1u, 0u, (uint8_t)'A', 0xABu);
  zr_assert_cell_ascii(ctx, &fb, 2u, 0u, (uint8_t)'B', 0xABu);
  zr_assert_cell_ascii(ctx, &fb, 3u, 0u, (uint8_t)'C', 0xABu);
  zr_assert_cell_ascii(ctx, &fb, 4u, 0u, (uint8_t)'D', 0xABu);
  zr_assert_cell_ascii(ctx, &fb, 5u, 0u, (uint8_t)'E', 0xABu);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_blit_rect_preserves_hyperlink_metadata) {
  uint8_t bytes[1024];
  zr_dl_builder_t b;
  const uint8_t txt[] = "XYZ";
  const uint8_t uri[] = "https://x.y";
  const uint8_t id[] = "id42";
  zr_test_style_wire_t s = zr_style_wire_plain(0x45u);
  zr_fb_t fb;

  s.link_uri_ref = 2u;
  s.link_id_ref = 3u;

  zr_builder_init(&b, bytes, sizeof(bytes));
  ZR_ASSERT_TRUE(zr_builder_cmd_clear(&b) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 1u, txt, 3u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 2u, uri, (uint32_t)sizeof(uri) - 1u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 3u, id, (uint32_t)sizeof(id) - 1u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 0, 0, 1u, 0u, 3u, &s) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_blit_rect(&b, 0, 0, 3, 1, 0, 1) != 0u);
  const size_t len = zr_builder_finish(&b, ZR_DRAWLIST_VERSION_V1);

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 2u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_drawlist(bytes, len, &fb), ZR_OK);

  for (uint32_t x = 0u; x < 3u; x++) {
    const zr_cell_t* src = zr_fb_cell_const(&fb, x, 0u);
    const zr_cell_t* dst = zr_fb_cell_const(&fb, x, 1u);
    const uint8_t* out_uri = NULL;
    size_t out_uri_len = 0u;
    const uint8_t* out_id = NULL;
    size_t out_id_len = 0u;

    ZR_ASSERT_TRUE(src != NULL && dst != NULL);
    ZR_ASSERT_TRUE(src->style.link_ref != 0u);
    ZR_ASSERT_EQ_U32(dst->style.link_ref, src->style.link_ref);
    ZR_ASSERT_EQ_U32(zr_fb_link_lookup(&fb, dst->style.link_ref, &out_uri, &out_uri_len, &out_id, &out_id_len), ZR_OK);
    ZR_ASSERT_EQ_U32(out_uri_len, sizeof(uri) - 1u);
    ZR_ASSERT_MEMEQ(out_uri, uri, sizeof(uri) - 1u);
    ZR_ASSERT_EQ_U32(out_id_len, sizeof(id) - 1u);
    ZR_ASSERT_MEMEQ(out_id, id, sizeof(id) - 1u);
  }

  zr_assert_cell_ascii(ctx, &fb, 0u, 1u, (uint8_t)'X', 0x45u);
  zr_assert_cell_ascii(ctx, &fb, 1u, 1u, (uint8_t)'Y', 0x45u);
  zr_assert_cell_ascii(ctx, &fb, 2u, 1u, (uint8_t)'Z', 0x45u);
  zr_fb_release(&fb);
}

ZR_TEST_UNIT(drawlist_blit_rect_handles_border_aligned_rectangles) {
  uint8_t bytes[1024];
  zr_dl_builder_t b;
  const uint8_t row0[] = "ab";
  const uint8_t row1[] = "cd";
  const zr_test_style_wire_t s0 = zr_style_wire_plain(0x11u);
  const zr_test_style_wire_t s1 = zr_style_wire_plain(0x22u);
  zr_fb_t fb;

  zr_builder_init(&b, bytes, sizeof(bytes));
  ZR_ASSERT_TRUE(zr_builder_cmd_clear(&b) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 1u, row0, 2u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_def_string(&b, 2u, row1, 2u) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 0, 0, 1u, 0u, 2u, &s0) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_draw_text(&b, 0, 1, 2u, 0u, 2u, &s1) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_blit_rect(&b, 0, 0, 2, 2, 2, 1) != 0u);
  ZR_ASSERT_TRUE(zr_builder_cmd_blit_rect(&b, 2, 1, 2, 2, 0, 0) != 0u);
  const size_t len = zr_builder_finish(&b, ZR_DRAWLIST_VERSION_V1);

  ZR_ASSERT_EQ_U32(zr_fb_init(&fb, 4u, 3u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_fb_clear(&fb, NULL), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_exec_drawlist(bytes, len, &fb), ZR_OK);

  zr_assert_cell_ascii(ctx, &fb, 2u, 1u, (uint8_t)'a', 0x11u);
  zr_assert_cell_ascii(ctx, &fb, 3u, 1u, (uint8_t)'b', 0x11u);
  zr_assert_cell_ascii(ctx, &fb, 2u, 2u, (uint8_t)'c', 0x22u);
  zr_assert_cell_ascii(ctx, &fb, 3u, 2u, (uint8_t)'d', 0x22u);
  zr_assert_cell_ascii(ctx, &fb, 0u, 0u, (uint8_t)'a', 0x11u);
  zr_assert_cell_ascii(ctx, &fb, 1u, 0u, (uint8_t)'b', 0x11u);
  zr_assert_cell_ascii(ctx, &fb, 0u, 1u, (uint8_t)'c', 0x22u);
  zr_assert_cell_ascii(ctx, &fb, 1u, 1u, (uint8_t)'d', 0x22u);
  zr_fb_release(&fb);
}
