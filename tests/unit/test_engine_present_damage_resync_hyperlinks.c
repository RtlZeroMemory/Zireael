/*
  tests/unit/test_engine_present_damage_resync_hyperlinks.c — Present resync correctness with hyperlinks.

  Why: engine_present() may resync fb_prev from fb_next using damage rectangles
  to avoid full-frame clones. Hyperlink equality is based on URI/ID targets, so
  two frames can be visually identical even when link_ref indices differ due to
  different interning orders. Present commit must not corrupt fb_prev metadata
  in these cases.
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"
#include "zr/zr_drawlist.h"
#include "zr/zr_version.h"

#include "unit/mock_platform.h"

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

static uint8_t zr_builder_cmd_draw_text(zr_dl_builder_t* b, int32_t x, int32_t y, uint32_t string_id, uint32_t byte_len,
                                        const zr_test_style_wire_t* style) {
  if (!style || !zr_builder_reserve(b, 60u)) {
    return 0u;
  }
  zr_builder_cmd_header(b, ZR_DL_OP_DRAW_TEXT, 60u);
  zr_wi32(b->out, &b->at, x);
  zr_wi32(b->out, &b->at, y);
  zr_w32(b->out, &b->at, string_id);
  zr_w32(b->out, &b->at, 0u);
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

static zr_test_style_wire_t zr_style_wire_link(uint32_t fg, uint32_t link_uri_ref) {
  zr_test_style_wire_t s;
  memset(&s, 0, sizeof(s));
  s.fg = fg;
  s.link_uri_ref = link_uri_ref;
  return s;
}

static size_t zr_build_frame1(uint8_t* out, size_t out_cap) {
  zr_dl_builder_t b;
  zr_builder_init(&b, out, out_cap);

  const uint8_t a_txt[] = "A";
  const uint8_t b_txt[] = "B";
  const uint8_t uri_a[] = "https://a.example";
  const uint8_t uri_b[] = "https://b.example";

  if (zr_builder_cmd_clear(&b) == 0u) {
    return 0u;
  }
  if (zr_builder_cmd_def_string(&b, 1u, a_txt, 1u) == 0u) {
    return 0u;
  }
  if (zr_builder_cmd_def_string(&b, 2u, b_txt, 1u) == 0u) {
    return 0u;
  }
  if (zr_builder_cmd_def_string(&b, 10u, uri_a, (uint32_t)sizeof(uri_a) - 1u) == 0u) {
    return 0u;
  }
  if (zr_builder_cmd_def_string(&b, 11u, uri_b, (uint32_t)sizeof(uri_b) - 1u) == 0u) {
    return 0u;
  }

  const zr_test_style_wire_t s_a = zr_style_wire_link(0x11u, 10u);
  const zr_test_style_wire_t s_b = zr_style_wire_link(0x11u, 11u);
  if (zr_builder_cmd_draw_text(&b, 0, 0, 1u, 1u, &s_a) == 0u) {
    return 0u;
  }
  if (zr_builder_cmd_draw_text(&b, 1, 0, 2u, 1u, &s_b) == 0u) {
    return 0u;
  }

  return zr_builder_finish(&b, ZR_DRAWLIST_VERSION_V1);
}

static size_t zr_build_frame2(uint8_t* out, size_t out_cap) {
  zr_dl_builder_t b;
  zr_builder_init(&b, out, out_cap);

  if (zr_builder_cmd_clear(&b) == 0u) {
    return 0u;
  }

  const zr_test_style_wire_t s_a = zr_style_wire_link(0x11u, 10u);
  const zr_test_style_wire_t s_b = zr_style_wire_link(0x11u, 11u);

  /* Same final content, but swap draw order to reorder link interning. */
  if (zr_builder_cmd_draw_text(&b, 1, 0, 2u, 1u, &s_b) == 0u) {
    return 0u;
  }
  if (zr_builder_cmd_draw_text(&b, 0, 0, 1u, 1u, &s_a) == 0u) {
    return 0u;
  }

  return zr_builder_finish(&b, ZR_DRAWLIST_VERSION_V1);
}

static void zr_get_metrics(zr_test_ctx_t* ctx, zr_engine_t* e, zr_metrics_t* out) {
  ZR_ASSERT_TRUE(out != NULL);
  memset(out, 0, sizeof(*out));
  out->struct_size = (uint32_t)sizeof(*out);
  ZR_ASSERT_EQ_U32(engine_get_metrics(e, out), ZR_OK);
}

ZR_TEST_UNIT(engine_present_damage_resync_preserves_hyperlink_targets_when_refs_reorder) {
  enum { ZR_COLS = 2u, ZR_ROWS = 10u };
  uint8_t frame1[512];
  uint8_t frame2[512];
  const size_t frame1_len = zr_build_frame1(frame1, sizeof(frame1));
  const size_t frame2_len = zr_build_frame2(frame2, sizeof(frame2));
  ZR_ASSERT_TRUE(frame1_len != 0u);
  ZR_ASSERT_TRUE(frame2_len != 0u);

  mock_plat_reset();
  mock_plat_set_size(ZR_COLS, ZR_ROWS);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  ZR_ASSERT_EQ_U32(engine_submit_drawlist(e, frame1, (int)frame1_len), ZR_OK);
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);

  ZR_ASSERT_EQ_U32(engine_submit_drawlist(e, frame2, (int)frame2_len), ZR_OK);
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);

  /* No new drawlist: present should be a no-op diff (no damage). */
  ZR_ASSERT_EQ_U32(engine_present(e), ZR_OK);
  zr_metrics_t m;
  zr_get_metrics(ctx, e, &m);
  ZR_ASSERT_EQ_U32(m.damage_cells_last_frame, 0u);
  ZR_ASSERT_EQ_U32(m.damage_rects_last_frame, 0u);
  ZR_ASSERT_EQ_U32((uint32_t)m.damage_full_frame, 0u);

  engine_destroy(e);
}
