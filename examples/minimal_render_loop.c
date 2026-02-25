/*
  examples/minimal_render_loop.c — Minimal render + poll loop.

  Why: Demonstrates the intended wrapper shape:
    - poll packed events into a caller buffer
    - submit a small drawlist (binary)
    - present (diff + single flush)
*/

#include "zr_example_common.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <zr/zr_config.h>
#include <zr/zr_drawlist.h>
#include <zr/zr_engine.h>
#include <zr/zr_event.h>
#include <zr/zr_result.h>
#include <zr/zr_version.h>

/* 'ZRDL' little-endian u32. */
#define ZR_EX_DL_MAGIC (0x4C44525Au)
#define ZR_EX_DL_HEADER_SIZE (64u)

static int zr_ex_build_drawlist(uint8_t* out, int out_cap, const char* status_line) {
  static const zr_dl_style_t kTitleStyle = {0x00FFFFFFu, 0x00000000u, 0u, 0u};
  static const zr_dl_style_t kStatusStyle = {0x00A0FFA0u, 0x00000000u, 0u, 0u};

  const char* title = "Zireael example: minimal render loop (press Esc)";
  const uint32_t title_len = (uint32_t)strlen(title);
  const uint32_t status_len = (uint32_t)strlen(status_line);

  uint8_t cmd_bytes[128];
  uint32_t cmd_len = 0u;

  /* --- CLEAR (8 bytes) --- */
  {
    zr_ex_le16_write(cmd_bytes + cmd_len + 0u, (uint16_t)ZR_DL_OP_CLEAR);
    zr_ex_le16_write(cmd_bytes + cmd_len + 2u, 0u);
    zr_ex_le32_write(cmd_bytes + cmd_len + 4u, 8u);
    cmd_len += 8u;
  }

  /* --- DRAW_TEXT: title (48 bytes) --- */
  {
    uint8_t* p = cmd_bytes + cmd_len;
    zr_ex_le16_write(p + 0u, (uint16_t)ZR_DL_OP_DRAW_TEXT);
    zr_ex_le16_write(p + 2u, 0u);
    zr_ex_le32_write(p + 4u, 48u);
    zr_ex_le32_write(p + 8u, 2u);  /* x */
    zr_ex_le32_write(p + 12u, 1u); /* y */
    zr_ex_le32_write(p + 16u, 0u); /* string_index */
    zr_ex_le32_write(p + 20u, 0u); /* byte_off */
    zr_ex_le32_write(p + 24u, title_len);
    zr_ex_le32_write(p + 28u, kTitleStyle.fg);
    zr_ex_le32_write(p + 32u, kTitleStyle.bg);
    zr_ex_le32_write(p + 36u, kTitleStyle.attrs);
    zr_ex_le32_write(p + 40u, 0u);
    zr_ex_le32_write(p + 44u, 0u);
    cmd_len += 48u;
  }

  /* --- DRAW_TEXT: status line (48 bytes) --- */
  {
    uint8_t* p = cmd_bytes + cmd_len;
    zr_ex_le16_write(p + 0u, (uint16_t)ZR_DL_OP_DRAW_TEXT);
    zr_ex_le16_write(p + 2u, 0u);
    zr_ex_le32_write(p + 4u, 48u);
    zr_ex_le32_write(p + 8u, 2u);  /* x */
    zr_ex_le32_write(p + 12u, 3u); /* y */
    zr_ex_le32_write(p + 16u, 1u); /* string_index */
    zr_ex_le32_write(p + 20u, 0u);
    zr_ex_le32_write(p + 24u, status_len);
    zr_ex_le32_write(p + 28u, kStatusStyle.fg);
    zr_ex_le32_write(p + 32u, kStatusStyle.bg);
    zr_ex_le32_write(p + 36u, kStatusStyle.attrs);
    zr_ex_le32_write(p + 40u, 0u);
    zr_ex_le32_write(p + 44u, 0u);
    cmd_len += 48u;
  }

  /*
    Drawlist memory layout for this example:
      [fixed header][command stream][string spans table][string bytes]
    Offsets below are absolute from start-of-buffer and 4-byte aligned.
  */
  const uint32_t cmd_off = ZR_EX_DL_HEADER_SIZE;
  const uint32_t strings_span_off = zr_ex_align4_u32(cmd_off + cmd_len);
  const uint32_t strings_bytes_off = zr_ex_align4_u32(strings_span_off + 2u * (uint32_t)sizeof(zr_dl_span_t));

  const uint32_t title_off = 0u;
  const uint32_t status_off = title_off + title_len;

  const uint32_t strings_bytes_len = title_len + status_len;
  const uint32_t total_size = zr_ex_align4_u32(strings_bytes_off + strings_bytes_len);
  if (total_size > (uint32_t)out_cap) {
    return -1;
  }

  memset(out, 0, (size_t)total_size);

  /* --- Header --- */
  zr_ex_le32_write(out + 0u, ZR_EX_DL_MAGIC);
  zr_ex_le32_write(out + 4u, ZR_DRAWLIST_VERSION_V1);
  zr_ex_le32_write(out + 8u, ZR_EX_DL_HEADER_SIZE);
  zr_ex_le32_write(out + 12u, total_size);

  zr_ex_le32_write(out + 16u, cmd_off);
  zr_ex_le32_write(out + 20u, cmd_len);
  zr_ex_le32_write(out + 24u, 3u);

  zr_ex_le32_write(out + 28u, strings_span_off);
  zr_ex_le32_write(out + 32u, 2u);
  zr_ex_le32_write(out + 36u, strings_bytes_off);
  zr_ex_le32_write(out + 40u, strings_bytes_len);

  zr_ex_le32_write(out + 44u, 0u);
  zr_ex_le32_write(out + 48u, 0u);
  zr_ex_le32_write(out + 52u, 0u);
  zr_ex_le32_write(out + 56u, 0u);
  zr_ex_le32_write(out + 60u, 0u);

  /* --- Command stream --- */
  memcpy(out + cmd_off, cmd_bytes, (size_t)cmd_len);

  /* --- Strings spans + bytes --- */
  zr_ex_le32_write(out + strings_span_off + 0u, title_off);
  zr_ex_le32_write(out + strings_span_off + 4u, title_len);
  zr_ex_le32_write(out + strings_span_off + 8u, status_off);
  zr_ex_le32_write(out + strings_span_off + 12u, status_len);

  memcpy(out + strings_bytes_off + title_off, title, title_len);
  memcpy(out + strings_bytes_off + status_off, status_line, status_len);

  return (int)total_size;
}

static bool zr_ex_batch_has_escape(const uint8_t* bytes, uint32_t len) {
  if (!bytes || len < (uint32_t)sizeof(zr_evbatch_header_t)) {
    return false;
  }

  const uint32_t magic = zr_ex_le32_read(bytes + 0u);
  const uint32_t version = zr_ex_le32_read(bytes + 4u);
  const uint32_t total_size = zr_ex_le32_read(bytes + 8u);
  if (magic != ZR_EV_MAGIC || version != ZR_EVENT_BATCH_VERSION_V1 || total_size > len) {
    return false;
  }

  uint32_t off = (uint32_t)sizeof(zr_evbatch_header_t);
  /* Scan packed records for KEY(ESC, DOWN), skipping unknown record types safely. */
  while (off + (uint32_t)sizeof(zr_ev_record_header_t) <= total_size) {
    const uint32_t type = zr_ex_le32_read(bytes + off + 0u);
    const uint32_t size = zr_ex_le32_read(bytes + off + 4u);
    if (size < (uint32_t)sizeof(zr_ev_record_header_t) || (off + size) > total_size) {
      return false;
    }

    if (type == (uint32_t)ZR_EV_KEY && size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t))) {
      const uint32_t key = zr_ex_le32_read(bytes + off + (uint32_t)sizeof(zr_ev_record_header_t) + 0u);
      const uint32_t action = zr_ex_le32_read(bytes + off + (uint32_t)sizeof(zr_ev_record_header_t) + 8u);
      if (key == (uint32_t)ZR_KEY_ESCAPE && action == (uint32_t)ZR_KEY_ACTION_DOWN) {
        return true;
      }
    }

    off += zr_ex_align4_u32(size);
  }

  return false;
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
  zr_result_t rc = engine_create(&e, &cfg);
  if (rc != ZR_OK) {
    fprintf(stderr, "engine_create failed: %d\n", (int)rc);
    return 1;
  }

  uint8_t event_buf[4096];
  uint8_t dl_buf[4096];
  const char* status_line = "No input yet.";

  for (;;) {
    const int n = engine_poll_events(e, 16, event_buf, (int)sizeof(event_buf));
    if (n < 0) {
      fprintf(stderr, "engine_poll_events failed: %d\n", n);
      break;
    }

    if (n > 0 && zr_ex_batch_has_escape(event_buf, (uint32_t)n)) {
      status_line = "Esc pressed. Exiting.";
    }

    const int dl_len = zr_ex_build_drawlist(dl_buf, (int)sizeof(dl_buf), status_line);
    if (dl_len < 0) {
      fprintf(stderr, "drawlist build failed\n");
      break;
    }

    rc = engine_submit_drawlist(e, dl_buf, dl_len);
    if (rc != ZR_OK) {
      fprintf(stderr, "engine_submit_drawlist failed: %d\n", (int)rc);
      break;
    }

    rc = engine_present(e);
    if (rc != ZR_OK) {
      fprintf(stderr, "engine_present failed: %d\n", (int)rc);
      break;
    }

    if (status_line[0] == 'E') { /* "Esc pressed. Exiting." */
      break;
    }
  }

  engine_destroy(e);
  return 0;
}
