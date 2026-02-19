/*
  tests/golden/golden_blit_subcell.c — Golden fixtures for sub-cell blitters.

  Why: Pins byte-exact serialized cell outputs for deterministic sub-cell
  rendering behavior across halfblock/quadrant/sextant/braille/canvas paths.
*/

#include "zr_test.h"

#include "golden/zr_golden.h"

#include "core/zr_blit.h"
#include "core/zr_drawlist.h"
#include "core/zr_framebuffer.h"
#include "unicode/zr_width.h"
#include "zr/zr_version.h"

#include <string.h>

static size_t zr_cell_serialize(const zr_cell_t* c, uint8_t* out, size_t cap) {
  size_t at = 0u;
  if (!c || !out || cap < (size_t)c->glyph_len + 10u) {
    return 0u;
  }
  out[at++] = c->glyph_len;
  for (uint8_t i = 0u; i < c->glyph_len; i++) {
    out[at++] = c->glyph[i];
  }
  out[at++] = c->width;
  out[at++] = (uint8_t)(c->style.fg_rgb & 0xFFu);
  out[at++] = (uint8_t)((c->style.fg_rgb >> 8u) & 0xFFu);
  out[at++] = (uint8_t)((c->style.fg_rgb >> 16u) & 0xFFu);
  out[at++] = (uint8_t)((c->style.fg_rgb >> 24u) & 0xFFu);
  out[at++] = (uint8_t)(c->style.bg_rgb & 0xFFu);
  out[at++] = (uint8_t)((c->style.bg_rgb >> 8u) & 0xFFu);
  out[at++] = (uint8_t)((c->style.bg_rgb >> 16u) & 0xFFu);
  out[at++] = (uint8_t)((c->style.bg_rgb >> 24u) & 0xFFu);
  return at;
}

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

static size_t zr_make_canvas_drawlist(uint8_t* out, const zr_dl_cmd_draw_canvas_t* cmd, const uint8_t* blob,
                                      uint32_t blob_len) {
  const uint32_t total = 64u + 40u + 8u + blob_len;
  size_t at = 0u;
  memset(out, 0, (size_t)total);

  zr_w32(out, &at, 0x4C44525Au);
  zr_w32(out, &at, ZR_DRAWLIST_VERSION_V4);
  zr_w32(out, &at, 64u);
  zr_w32(out, &at, total);
  zr_w32(out, &at, 64u);
  zr_w32(out, &at, 40u);
  zr_w32(out, &at, 2u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 0u);
  zr_w32(out, &at, 104u);
  zr_w32(out, &at, 1u);
  zr_w32(out, &at, 112u);
  zr_w32(out, &at, blob_len);
  zr_w32(out, &at, 0u);

  zr_cmd_header(out, &at, ZR_DL_OP_CLEAR, 8u);
  zr_cmd_header(out, &at, ZR_DL_OP_DRAW_CANVAS, 32u);
  zr_w16(out, &at, cmd->dst_col);
  zr_w16(out, &at, cmd->dst_row);
  zr_w16(out, &at, cmd->dst_cols);
  zr_w16(out, &at, cmd->dst_rows);
  zr_w16(out, &at, cmd->px_width);
  zr_w16(out, &at, cmd->px_height);
  zr_w32(out, &at, cmd->blob_offset);
  zr_w32(out, &at, cmd->blob_len);
  out[at++] = cmd->blitter;
  out[at++] = cmd->flags;
  zr_w16(out, &at, cmd->reserved);

  zr_w32(out, &at, 0u);
  zr_w32(out, &at, blob_len);
  memcpy(out + at, blob, blob_len);
  at += blob_len;
  return at;
}

ZR_TEST_GOLDEN(blit_halfblock_001_upper_half) {
  uint8_t pixels[8] = {255u, 0u, 0u, 255u, 0u, 0u, 255u, 255u};
  zr_blit_input_t in = {pixels, 1u, 2u, 4u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];
  uint8_t out[32];

  (void)zr_fb_init(&fb, 1u, 1u);
  (void)zr_fb_clear(&fb, NULL);
  (void)zr_fb_painter_begin(&p, &fb, stack, 2u);
  (void)zr_blit_halfblock(&p, (zr_rect_t){0, 0, 1, 1}, &in);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  const size_t n = zr_cell_serialize(c, out, sizeof(out));
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("blit_halfblock_001_upper_half", out, n) == 0);
  zr_fb_release(&fb);
}

ZR_TEST_GOLDEN(blit_quadrant_001_checkerboard) {
  uint8_t pixels[16] = {
      255u, 255u, 255u, 255u, 0u, 0u, 0u, 255u,
      0u,   0u,   0u,   255u, 255u, 255u, 255u, 255u,
  };
  zr_blit_input_t in = {pixels, 2u, 2u, 8u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];
  uint8_t out[32];

  (void)zr_fb_init(&fb, 1u, 1u);
  (void)zr_fb_clear(&fb, NULL);
  (void)zr_fb_painter_begin(&p, &fb, stack, 2u);
  (void)zr_blit_quadrant(&p, (zr_rect_t){0, 0, 1, 1}, &in);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  const size_t n = zr_cell_serialize(c, out, sizeof(out));
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("blit_quadrant_001_checkerboard", out, n) == 0);
  zr_fb_release(&fb);
}

ZR_TEST_GOLDEN(blit_sextant_001_left_column) {
  uint8_t pixels[24] = {
      255u, 255u, 255u, 255u, 0u, 0u, 0u, 255u,
      255u, 255u, 255u, 255u, 0u, 0u, 0u, 255u,
      255u, 255u, 255u, 255u, 0u, 0u, 0u, 255u,
  };
  zr_blit_input_t in = {pixels, 2u, 3u, 8u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];
  uint8_t out[32];

  (void)zr_fb_init(&fb, 1u, 1u);
  (void)zr_fb_clear(&fb, NULL);
  (void)zr_fb_painter_begin(&p, &fb, stack, 2u);
  (void)zr_blit_sextant(&p, (zr_rect_t){0, 0, 1, 1}, &in);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  const size_t n = zr_cell_serialize(c, out, sizeof(out));
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("blit_sextant_001_left_column", out, n) == 0);
  zr_fb_release(&fb);
}

ZR_TEST_GOLDEN(blit_braille_001_dot1) {
  uint8_t pixels[32] = {0u};
  for (int i = 0; i < 8; i++) {
    pixels[i * 4 + 3] = 255u;
  }
  pixels[0] = 255u;
  pixels[1] = 255u;
  pixels[2] = 255u;

  zr_blit_input_t in = {pixels, 2u, 4u, 8u};
  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];
  uint8_t out[32];

  (void)zr_fb_init(&fb, 1u, 1u);
  (void)zr_fb_clear(&fb, NULL);
  (void)zr_fb_painter_begin(&p, &fb, stack, 2u);
  (void)zr_blit_braille(&p, (zr_rect_t){0, 0, 1, 1}, &in);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  const size_t n = zr_cell_serialize(c, out, sizeof(out));
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("blit_braille_001_dot1", out, n) == 0);
  zr_fb_release(&fb);
}

ZR_TEST_GOLDEN(blit_drawlist_canvas_001_ascii) {
  uint8_t blob[4] = {12u, 34u, 56u, 255u};
  uint8_t bytes[160];
  uint8_t out[32];
  zr_dl_cmd_draw_canvas_t cmd = {0, 0, 1, 1, 1, 1, 0u, 4u, (uint8_t)ZR_BLIT_ASCII, 0u, 0u};
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  zr_cursor_state_t cursor = {0};
  zr_fb_t fb;

  const size_t len = zr_make_canvas_drawlist(bytes, &cmd, blob, 4u);
  cursor.x = -1;
  cursor.y = -1;
  cursor.shape = ZR_CURSOR_SHAPE_BLOCK;

  (void)zr_fb_init(&fb, 1u, 1u);
  (void)zr_fb_clear(&fb, NULL);
  ZR_ASSERT_EQ_U32(zr_dl_validate(bytes, len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_execute(&v, &fb, &lim, 4u, (uint32_t)ZR_WIDTH_EMOJI_WIDE, NULL, &cursor), ZR_OK);

  const zr_cell_t* c = zr_fb_cell_const(&fb, 0u, 0u);
  const size_t n = zr_cell_serialize(c, out, sizeof(out));
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("blit_drawlist_canvas_001_ascii", out, n) == 0);
  zr_fb_release(&fb);
}
