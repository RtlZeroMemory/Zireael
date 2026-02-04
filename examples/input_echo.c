/*
  examples/input_echo.c — Event-batch parsing and display.

  Why: Demonstrates wrapper-side parsing of the packed event batch by reading
  little-endian fields and skipping unknown record types by size.
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

#define ZR_EX_DL_MAGIC (0x4C44525Au)
#define ZR_EX_DL_HEADER_SIZE (64u)

#define ZR_EX_LINES_MAX (16u)
#define ZR_EX_LINE_CAP (96u)

typedef struct zr_ex_lines_t {
  char lines[ZR_EX_LINES_MAX][ZR_EX_LINE_CAP];
  uint32_t count;
} zr_ex_lines_t;

static void zr_ex_lines_push(zr_ex_lines_t* ls, const char* s) {
  if (!ls || !s) {
    return;
  }

  if (ls->count < ZR_EX_LINES_MAX) {
    snprintf(ls->lines[ls->count], ZR_EX_LINE_CAP, "%s", s);
    ls->count++;
    return;
  }

  for (uint32_t i = 1u; i < ZR_EX_LINES_MAX; i++) {
    memcpy(ls->lines[i - 1u], ls->lines[i], ZR_EX_LINE_CAP);
  }
  snprintf(ls->lines[ZR_EX_LINES_MAX - 1u], ZR_EX_LINE_CAP, "%s", s);
}

static bool zr_ex_parse_events(zr_ex_lines_t* lines, const uint8_t* bytes, uint32_t len) {
  if (!lines || !bytes || len < (uint32_t)sizeof(zr_evbatch_header_t)) {
    return false;
  }

  const uint32_t magic = zr_ex_le32_read(bytes + 0u);
  const uint32_t version = zr_ex_le32_read(bytes + 4u);
  const uint32_t total_size = zr_ex_le32_read(bytes + 8u);
  if (magic != ZR_EV_MAGIC || version != ZR_EVENT_BATCH_VERSION_V1 || total_size > len) {
    return false;
  }

  uint32_t off = (uint32_t)sizeof(zr_evbatch_header_t);
  while (off + (uint32_t)sizeof(zr_ev_record_header_t) <= total_size) {
    const uint32_t type = zr_ex_le32_read(bytes + off + 0u);
    const uint32_t size = zr_ex_le32_read(bytes + off + 4u);
    if (size < (uint32_t)sizeof(zr_ev_record_header_t) || (off + size) > total_size) {
      return false;
    }

    char buf[ZR_EX_LINE_CAP];
    buf[0] = '\0';

    const uint32_t payload_off = off + (uint32_t)sizeof(zr_ev_record_header_t);
    if (type == (uint32_t)ZR_EV_KEY && size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t))) {
      const uint32_t key = zr_ex_le32_read(bytes + payload_off + 0u);
      const uint32_t mods = zr_ex_le32_read(bytes + payload_off + 4u);
      const uint32_t action = zr_ex_le32_read(bytes + payload_off + 8u);
      snprintf(buf, sizeof(buf), "KEY key=%u mods=0x%X action=%u", key, (unsigned)mods, action);
    } else if (type == (uint32_t)ZR_EV_TEXT && size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_text_t))) {
      const uint32_t cp = zr_ex_le32_read(bytes + payload_off + 0u);
      snprintf(buf, sizeof(buf), "TEXT U+%04X", (unsigned)cp);
    } else if (type == (uint32_t)ZR_EV_PASTE && size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_paste_t))) {
      const uint32_t byte_len = zr_ex_le32_read(bytes + payload_off + 0u);
      snprintf(buf, sizeof(buf), "PASTE bytes=%u", (unsigned)byte_len);
    } else if (type == (uint32_t)ZR_EV_MOUSE && size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_mouse_t))) {
      const int32_t x = (int32_t)zr_ex_le32_read(bytes + payload_off + 0u);
      const int32_t y = (int32_t)zr_ex_le32_read(bytes + payload_off + 4u);
      const uint32_t kind = zr_ex_le32_read(bytes + payload_off + 8u);
      snprintf(buf, sizeof(buf), "MOUSE kind=%u x=%d y=%d", (unsigned)kind, (int)x, (int)y);
    } else if (type == (uint32_t)ZR_EV_RESIZE && size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_resize_t))) {
      const uint32_t cols = zr_ex_le32_read(bytes + payload_off + 0u);
      const uint32_t rows = zr_ex_le32_read(bytes + payload_off + 4u);
      snprintf(buf, sizeof(buf), "RESIZE cols=%u rows=%u", (unsigned)cols, (unsigned)rows);
    } else if (type == (uint32_t)ZR_EV_TICK && size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_tick_t))) {
      const uint32_t dt_ms = zr_ex_le32_read(bytes + payload_off + 0u);
      snprintf(buf, sizeof(buf), "TICK dt_ms=%u", (unsigned)dt_ms);
    } else if (type == (uint32_t)ZR_EV_USER && size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_user_t))) {
      const uint32_t tag = zr_ex_le32_read(bytes + payload_off + 0u);
      const uint32_t byte_len = zr_ex_le32_read(bytes + payload_off + 4u);
      snprintf(buf, sizeof(buf), "USER tag=%u bytes=%u", (unsigned)tag, (unsigned)byte_len);
    } else {
      snprintf(buf, sizeof(buf), "UNKNOWN type=%u size=%u", (unsigned)type, (unsigned)size);
    }

    zr_ex_lines_push(lines, buf);
    off += zr_ex_align4_u32(size);
  }

  return true;
}

static int zr_ex_build_lines_drawlist(uint8_t* out, int out_cap, const zr_ex_lines_t* lines) {
  static const zr_dl_style_t kTextStyle = {0x00FFFFFFu, 0x00000000u, 0u, 0u};

  if (!lines) {
    return -1;
  }

  uint8_t cmd_bytes[1024];
  uint32_t cmd_len = 0u;
  uint32_t cmd_count = 0u;

  /* CLEAR */
  {
    zr_ex_le16_write(cmd_bytes + cmd_len + 0u, (uint16_t)ZR_DL_OP_CLEAR);
    zr_ex_le16_write(cmd_bytes + cmd_len + 2u, 0u);
    zr_ex_le32_write(cmd_bytes + cmd_len + 4u, 8u);
    cmd_len += 8u;
    cmd_count++;
  }

  /* One DRAW_TEXT per line. */
  for (uint32_t i = 0u; i < lines->count; i++) {
    const uint32_t line_len = (uint32_t)strlen(lines->lines[i]);
    uint8_t* p = cmd_bytes + cmd_len;
    zr_ex_le16_write(p + 0u, (uint16_t)ZR_DL_OP_DRAW_TEXT);
    zr_ex_le16_write(p + 2u, 0u);
    zr_ex_le32_write(p + 4u, 48u);
    zr_ex_le32_write(p + 8u, 2u);
    zr_ex_le32_write(p + 12u, 2u + (int32_t)i);
    zr_ex_le32_write(p + 16u, i);
    zr_ex_le32_write(p + 20u, 0u);
    zr_ex_le32_write(p + 24u, line_len);
    zr_ex_le32_write(p + 28u, kTextStyle.fg);
    zr_ex_le32_write(p + 32u, kTextStyle.bg);
    zr_ex_le32_write(p + 36u, kTextStyle.attrs);
    zr_ex_le32_write(p + 40u, 0u);
    zr_ex_le32_write(p + 44u, 0u);
    cmd_len += 48u;
    cmd_count++;
  }

  const uint32_t cmd_off = ZR_EX_DL_HEADER_SIZE;
  const uint32_t strings_span_off = zr_ex_align4_u32(cmd_off + cmd_len);
  const uint32_t strings_bytes_off = zr_ex_align4_u32(strings_span_off + lines->count * (uint32_t)sizeof(zr_dl_span_t));

  uint32_t strings_bytes_len = 0u;
  for (uint32_t i = 0u; i < lines->count; i++) {
    strings_bytes_len += (uint32_t)strlen(lines->lines[i]);
  }

  const uint32_t total_size = zr_ex_align4_u32(strings_bytes_off + strings_bytes_len);
  if (total_size > (uint32_t)out_cap) {
    return -1;
  }

  memset(out, 0, (size_t)total_size);

  zr_ex_le32_write(out + 0u, ZR_EX_DL_MAGIC);
  zr_ex_le32_write(out + 4u, ZR_DRAWLIST_VERSION_V1);
  zr_ex_le32_write(out + 8u, ZR_EX_DL_HEADER_SIZE);
  zr_ex_le32_write(out + 12u, total_size);

  zr_ex_le32_write(out + 16u, cmd_off);
  zr_ex_le32_write(out + 20u, cmd_len);
  zr_ex_le32_write(out + 24u, cmd_count);

  zr_ex_le32_write(out + 28u, strings_span_off);
  zr_ex_le32_write(out + 32u, lines->count);
  zr_ex_le32_write(out + 36u, strings_bytes_off);
  zr_ex_le32_write(out + 40u, strings_bytes_len);

  zr_ex_le32_write(out + 44u, 0u);
  zr_ex_le32_write(out + 48u, 0u);
  zr_ex_le32_write(out + 52u, 0u);
  zr_ex_le32_write(out + 56u, 0u);
  zr_ex_le32_write(out + 60u, 0u);

  memcpy(out + cmd_off, cmd_bytes, (size_t)cmd_len);

  uint32_t str_off = 0u;
  for (uint32_t i = 0u; i < lines->count; i++) {
    const uint32_t len = (uint32_t)strlen(lines->lines[i]);
    zr_ex_le32_write(out + strings_span_off + i * 8u + 0u, str_off);
    zr_ex_le32_write(out + strings_span_off + i * 8u + 4u, len);
    memcpy(out + strings_bytes_off + str_off, lines->lines[i], len);
    str_off += len;
  }

  return (int)total_size;
}

static bool zr_ex_should_exit_on_escape(const uint8_t* bytes, uint32_t len) {
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

  zr_ex_lines_t lines = {0};
  zr_ex_lines_push(&lines, "Zireael input echo (press Esc)");

  uint8_t event_buf[4096];
  uint8_t dl_buf[8192];

  for (;;) {
    const int n = engine_poll_events(e, 16, event_buf, (int)sizeof(event_buf));
    if (n < 0) {
      fprintf(stderr, "engine_poll_events failed: %d\n", n);
      break;
    }

    if (n > 0) {
      (void)zr_ex_parse_events(&lines, event_buf, (uint32_t)n);
      if (zr_ex_should_exit_on_escape(event_buf, (uint32_t)n)) {
        zr_ex_lines_push(&lines, "Esc pressed. Exiting.");
      }
    }

    const int dl_len = zr_ex_build_lines_drawlist(dl_buf, (int)sizeof(dl_buf), &lines);
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

    if (lines.count > 0u && strcmp(lines.lines[lines.count - 1u], "Esc pressed. Exiting.") == 0) {
      break;
    }
  }

  engine_destroy(e);
  return 0;
}

