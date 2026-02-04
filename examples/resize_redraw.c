/*
  examples/resize_redraw.c — Resize handling + redraw.

  Why: Demonstrates that wrappers should treat resize as an event (packed into
  the event batch) and rebuild drawlists for the new viewport.
*/

#include "zr_example_common.h"

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

static int zr_ex_build_drawlist(uint8_t* out, int out_cap, const char* line0, const char* line1) {
  static const zr_dl_style_t kTextStyle = {0x00FFFFFFu, 0x00000000u, 0u, 0u};

  const uint32_t len0 = (uint32_t)strlen(line0);
  const uint32_t len1 = (uint32_t)strlen(line1);

  uint8_t cmd_bytes[160];
  uint32_t cmd_len = 0u;

  /* CLEAR */
  zr_ex_le16_write(cmd_bytes + cmd_len + 0u, (uint16_t)ZR_DL_OP_CLEAR);
  zr_ex_le16_write(cmd_bytes + cmd_len + 2u, 0u);
  zr_ex_le32_write(cmd_bytes + cmd_len + 4u, 8u);
  cmd_len += 8u;

  /* DRAW_TEXT line 0 */
  {
    uint8_t* p = cmd_bytes + cmd_len;
    zr_ex_le16_write(p + 0u, (uint16_t)ZR_DL_OP_DRAW_TEXT);
    zr_ex_le16_write(p + 2u, 0u);
    zr_ex_le32_write(p + 4u, 48u);
    zr_ex_le32_write(p + 8u, 2u);
    zr_ex_le32_write(p + 12u, 1u);
    zr_ex_le32_write(p + 16u, 0u);
    zr_ex_le32_write(p + 20u, 0u);
    zr_ex_le32_write(p + 24u, len0);
    zr_ex_le32_write(p + 28u, kTextStyle.fg);
    zr_ex_le32_write(p + 32u, kTextStyle.bg);
    zr_ex_le32_write(p + 36u, kTextStyle.attrs);
    zr_ex_le32_write(p + 40u, 0u);
    zr_ex_le32_write(p + 44u, 0u);
    cmd_len += 48u;
  }

  /* DRAW_TEXT line 1 */
  {
    uint8_t* p = cmd_bytes + cmd_len;
    zr_ex_le16_write(p + 0u, (uint16_t)ZR_DL_OP_DRAW_TEXT);
    zr_ex_le16_write(p + 2u, 0u);
    zr_ex_le32_write(p + 4u, 48u);
    zr_ex_le32_write(p + 8u, 2u);
    zr_ex_le32_write(p + 12u, 3u);
    zr_ex_le32_write(p + 16u, 1u);
    zr_ex_le32_write(p + 20u, 0u);
    zr_ex_le32_write(p + 24u, len1);
    zr_ex_le32_write(p + 28u, kTextStyle.fg);
    zr_ex_le32_write(p + 32u, kTextStyle.bg);
    zr_ex_le32_write(p + 36u, kTextStyle.attrs);
    zr_ex_le32_write(p + 40u, 0u);
    zr_ex_le32_write(p + 44u, 0u);
    cmd_len += 48u;
  }

  const uint32_t cmd_off = ZR_EX_DL_HEADER_SIZE;
  const uint32_t strings_span_off = zr_ex_align4_u32(cmd_off + cmd_len);
  const uint32_t strings_bytes_off = zr_ex_align4_u32(strings_span_off + 2u * (uint32_t)sizeof(zr_dl_span_t));

  const uint32_t off0 = 0u;
  const uint32_t off1 = off0 + len0;
  const uint32_t strings_bytes_len = len0 + len1;

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

  memcpy(out + cmd_off, cmd_bytes, (size_t)cmd_len);

  zr_ex_le32_write(out + strings_span_off + 0u, off0);
  zr_ex_le32_write(out + strings_span_off + 4u, len0);
  zr_ex_le32_write(out + strings_span_off + 8u, off1);
  zr_ex_le32_write(out + strings_span_off + 12u, len1);

  memcpy(out + strings_bytes_off + off0, line0, len0);
  memcpy(out + strings_bytes_off + off1, line1, len1);

  return (int)total_size;
}

static void zr_ex_scan_resize(const uint8_t* bytes, uint32_t len, uint32_t* io_cols, uint32_t* io_rows) {
  if (!bytes || len < (uint32_t)sizeof(zr_evbatch_header_t) || !io_cols || !io_rows) {
    return;
  }

  const uint32_t magic = zr_ex_le32_read(bytes + 0u);
  const uint32_t version = zr_ex_le32_read(bytes + 4u);
  const uint32_t total_size = zr_ex_le32_read(bytes + 8u);
  if (magic != ZR_EV_MAGIC || version != ZR_EVENT_BATCH_VERSION_V1 || total_size > len) {
    return;
  }

  uint32_t off = (uint32_t)sizeof(zr_evbatch_header_t);
  while (off + (uint32_t)sizeof(zr_ev_record_header_t) <= total_size) {
    const uint32_t type = zr_ex_le32_read(bytes + off + 0u);
    const uint32_t size = zr_ex_le32_read(bytes + off + 4u);
    if (size < (uint32_t)sizeof(zr_ev_record_header_t) || (off + size) > total_size) {
      return;
    }

    if (type == (uint32_t)ZR_EV_RESIZE && size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_resize_t))) {
      const uint32_t payload_off = off + (uint32_t)sizeof(zr_ev_record_header_t);
      *io_cols = zr_ex_le32_read(bytes + payload_off + 0u);
      *io_rows = zr_ex_le32_read(bytes + payload_off + 4u);
    }

    off += zr_ex_align4_u32(size);
  }
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

  uint32_t cols = 0u;
  uint32_t rows = 0u;

  uint8_t event_buf[4096];
  uint8_t dl_buf[4096];

  for (;;) {
    const int n = engine_poll_events(e, 64, event_buf, (int)sizeof(event_buf));
    if (n < 0) {
      fprintf(stderr, "engine_poll_events failed: %d\n", n);
      break;
    }

    if (n > 0) {
      zr_ex_scan_resize(event_buf, (uint32_t)n, &cols, &rows);
      /* Exit on Esc (same helper as minimal example). */
      if (n > 0) {
        const uint32_t magic = zr_ex_le32_read(event_buf + 0u);
        const uint32_t version = zr_ex_le32_read(event_buf + 4u);
        const uint32_t total_size = zr_ex_le32_read(event_buf + 8u);
        if (magic == ZR_EV_MAGIC && version == ZR_EVENT_BATCH_VERSION_V1 && total_size <= (uint32_t)n) {
          uint32_t off = (uint32_t)sizeof(zr_evbatch_header_t);
          while (off + (uint32_t)sizeof(zr_ev_record_header_t) <= total_size) {
            const uint32_t type = zr_ex_le32_read(event_buf + off + 0u);
            const uint32_t size = zr_ex_le32_read(event_buf + off + 4u);
            if (size < (uint32_t)sizeof(zr_ev_record_header_t) || (off + size) > total_size) {
              break;
            }
            if (type == (uint32_t)ZR_EV_KEY &&
                size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t))) {
              const uint32_t key = zr_ex_le32_read(event_buf + off + (uint32_t)sizeof(zr_ev_record_header_t) + 0u);
              const uint32_t action = zr_ex_le32_read(event_buf + off + (uint32_t)sizeof(zr_ev_record_header_t) + 8u);
              if (key == (uint32_t)ZR_KEY_ESCAPE && action == (uint32_t)ZR_KEY_ACTION_DOWN) {
                goto exit_loop;
              }
            }
            off += zr_ex_align4_u32(size);
          }
        }
      }
    }

    char line0[96];
    char line1[96];
    snprintf(line0, sizeof(line0), "Zireael resize + redraw (press Esc)");
    snprintf(line1, sizeof(line1), "Last resize: cols=%u rows=%u", (unsigned)cols, (unsigned)rows);

    const int dl_len = zr_ex_build_drawlist(dl_buf, (int)sizeof(dl_buf), line0, line1);
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
  }

exit_loop:
  engine_destroy(e);
  return 0;
}
