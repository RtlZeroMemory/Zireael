/*
  examples/example.c — Minimal Zireael embedding example.

  Why: Demonstrates the public C ABI surface end-to-end:
    - create engine
    - submit a tiny drawlist
    - present a frame
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <zr/zr_config.h>
#include <zr/zr_drawlist.h>
#include <zr/zr_engine.h>
#include <zr/zr_result.h>

static void le32_write(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v);
  p[1] = (uint8_t)(v >> 8u);
  p[2] = (uint8_t)(v >> 16u);
  p[3] = (uint8_t)(v >> 24u);
}

static void le16_write(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v);
  p[1] = (uint8_t)(v >> 8u);
}

static uint32_t rgb_u32(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16u) | ((uint32_t)g << 8u) | (uint32_t)b;
}

static int build_hello_drawlist(uint8_t* out, int out_cap) {
  /*
    Drawlist v1 layout:
      [64B header][cmd bytes][string spans][string bytes]
  */
  static const uint32_t kMagic = 0x4C44525A; /* 'ZRDL' little-endian */
  static const uint32_t kHeaderSize = 64u;

  static const char kTitle[] = "Zireael example: hello";

  uint8_t cmds[128];
  int cmd_len = 0;

  /* CLEAR */
  {
    uint8_t* p = cmds + cmd_len;
    le16_write(p + 0, (uint16_t)ZR_DL_OP_CLEAR);
    le16_write(p + 2, 0);
    le32_write(p + 4, 8u);
    cmd_len += 8;
  }

  /* FILL_RECT (background) */
  {
    const zr_dl_style_t st = {0, rgb_u32(0, 0, 0), 0, 0};
    uint8_t*            p = cmds + cmd_len;
    le16_write(p + 0, (uint16_t)ZR_DL_OP_FILL_RECT);
    le16_write(p + 2, 0);
    le32_write(p + 4, 40u);
    le32_write(p + 8, 0u);
    le32_write(p + 12, 0u);
    le32_write(p + 16, 10000u);
    le32_write(p + 20, 10000u);
    le32_write(p + 24, st.fg);
    le32_write(p + 28, st.bg);
    le32_write(p + 32, st.attrs);
    le32_write(p + 36, 0u);
    cmd_len += 40;
  }

  /* DRAW_TEXT */
  {
    const zr_dl_style_t st = {rgb_u32(80, 250, 123), rgb_u32(0, 0, 0), 0, 0};
    uint8_t*            p = cmds + cmd_len;
    le16_write(p + 0, (uint16_t)ZR_DL_OP_DRAW_TEXT);
    le16_write(p + 2, 0);
    le32_write(p + 4, 48u);
    le32_write(p + 8, 2u);
    le32_write(p + 12, 1u);
    le32_write(p + 16, 0u);                    /* string_index */
    le32_write(p + 20, 0u);                    /* byte_off */
    le32_write(p + 24, (uint32_t)sizeof(kTitle) - 1u); /* byte_len */
    le32_write(p + 28, st.fg);
    le32_write(p + 32, st.bg);
    le32_write(p + 36, st.attrs);
    le32_write(p + 40, 0u); /* style.reserved0 */
    le32_write(p + 44, 0u); /* cmd.reserved0 */
    cmd_len += 48;
  }

  const uint32_t cmd_off = kHeaderSize;
  const uint32_t cmd_bytes = (uint32_t)cmd_len;

  const uint32_t strings_span_off = cmd_off + cmd_bytes;
  const uint32_t strings_bytes_off = strings_span_off + 8u;
  const uint32_t strings_bytes_len = (uint32_t)sizeof(kTitle) - 1u;

  const uint32_t total_size = strings_bytes_off + strings_bytes_len;
  if (total_size > (uint32_t)out_cap) {
    return -1;
  }

  memset(out, 0, (size_t)total_size);

  /* Header */
  le32_write(out + 0, kMagic);
  le32_write(out + 4, ZR_DRAWLIST_VERSION_V1);
  le32_write(out + 8, kHeaderSize);
  le32_write(out + 12, total_size);
  le32_write(out + 16, cmd_off);
  le32_write(out + 20, cmd_bytes);
  le32_write(out + 24, 3u); /* cmd_count */
  le32_write(out + 28, strings_span_off);
  le32_write(out + 32, 1u); /* strings_count */
  le32_write(out + 36, strings_bytes_off);
  le32_write(out + 40, strings_bytes_len);
  le32_write(out + 44, 0u); /* blobs_span_offset */
  le32_write(out + 48, 0u); /* blobs_count */
  le32_write(out + 52, 0u); /* blobs_bytes_offset */
  le32_write(out + 56, 0u); /* blobs_bytes_len */
  le32_write(out + 60, 0u); /* reserved0 */

  /* Commands */
  memcpy(out + cmd_off, cmds, (size_t)cmd_len);

  /* String span + bytes */
  le32_write(out + strings_span_off + 0, 0u);
  le32_write(out + strings_span_off + 4, strings_bytes_len);
  memcpy(out + strings_bytes_off, kTitle, strings_bytes_len);

  return (int)total_size;
}

int main(void) {
  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.requested_engine_abi_major = ZR_ENGINE_ABI_MAJOR;
  cfg.requested_engine_abi_minor = ZR_ENGINE_ABI_MINOR;
  cfg.requested_engine_abi_patch = ZR_ENGINE_ABI_PATCH;
  cfg.requested_drawlist_version = ZR_DRAWLIST_VERSION_V1;
  cfg.requested_event_batch_version = ZR_EVENT_BATCH_VERSION_V1;
  cfg.plat.requested_color_mode = PLAT_COLOR_MODE_RGB;

  zr_engine_t* e = NULL;
  zr_result_t  rc = engine_create(&e, &cfg);
  if (rc != ZR_OK) {
    fprintf(stderr, "engine_create failed: %d\n", (int)rc);
    return 1;
  }

  uint8_t dl[4096];
  int     dl_len = build_hello_drawlist(dl, (int)sizeof(dl));
  if (dl_len < 0) {
    fprintf(stderr, "drawlist build failed\n");
    engine_destroy(e);
    return 1;
  }

  rc = engine_submit_drawlist(e, dl, dl_len);
  if (rc != ZR_OK) {
    fprintf(stderr, "engine_submit_drawlist failed: %d\n", (int)rc);
    engine_destroy(e);
    return 1;
  }
  rc = engine_present(e);
  if (rc != ZR_OK) {
    fprintf(stderr, "engine_present failed: %d\n", (int)rc);
    engine_destroy(e);
    return 1;
  }

  engine_destroy(e);
  return 0;
}
