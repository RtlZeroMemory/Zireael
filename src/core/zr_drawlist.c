/*
  src/core/zr_drawlist.c — Drawlist validator + executor (v1 + v2).

  Why: Validates wrapper-provided drawlist bytes (bounds/caps/version) and
  executes deterministic drawing into the framebuffer without UB.
  Invariants:
    - Offsets/sizes validated before any derived pointer is created.
    - Unaligned reads use safe helpers (no type-punning casts).
*/

#include "core/zr_drawlist.h"

#include "core/zr_framebuffer.h"
#include "core/zr_version.h"

#include "unicode/zr_grapheme.h"
#include "unicode/zr_utf8.h"
#include "unicode/zr_width.h"

#include "util/zr_bytes.h"
#include "util/zr_checked.h"

#include <string.h>

/* Drawlist v1 format identifiers. */
#define ZR_DL_MAGIC 0x4C44525Au /* 'ZRDL' as little-endian u32 */

/* Alignment requirement for drawlist sections. */
#define ZR_DL_ALIGNMENT 4u

static bool zr_is_aligned4_u32(uint32_t v) {
  return (v & (ZR_DL_ALIGNMENT - 1u)) == 0u;
}

static bool zr_checked_add_u32_to_size(uint32_t a, uint32_t b, size_t* out) {
  uint32_t sum = 0u;
  if (!zr_checked_add_u32(a, b, &sum)) {
    return false;
  }
  *out = (size_t)sum;
  return true;
}

static bool zr_checked_mul_u32_to_u32(uint32_t a, uint32_t b, uint32_t* out) {
  uint32_t prod = 0u;
  if (!zr_checked_mul_u32(a, b, &prod)) {
    return false;
  }
  *out = prod;
  return true;
}

static zr_result_t zr_dl_read_i32le(zr_byte_reader_t* r, int32_t* out) {
  uint32_t tmp = 0u;
  if (!out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!zr_byte_reader_read_u32le(r, &tmp)) {
    return ZR_ERR_FORMAT;
  }
  *out = (int32_t)tmp;
  return ZR_OK;
}

static zr_result_t zr_dl_read_style(zr_byte_reader_t* r, zr_dl_style_t* out) {
  if (!out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!zr_byte_reader_read_u32le(r, &out->fg) || !zr_byte_reader_read_u32le(r, &out->bg) ||
      !zr_byte_reader_read_u32le(r, &out->attrs) || !zr_byte_reader_read_u32le(r, &out->reserved0)) {
    return ZR_ERR_FORMAT;
  }
  return ZR_OK;
}

static zr_result_t zr_dl_read_cmd_header(zr_byte_reader_t* r, zr_dl_cmd_header_t* out) {
  uint16_t opcode = 0u;
  uint16_t flags = 0u;
  uint32_t size = 0u;
  if (!out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!zr_byte_reader_read_u16le(r, &opcode) || !zr_byte_reader_read_u16le(r, &flags) ||
      !zr_byte_reader_read_u32le(r, &size)) {
    return ZR_ERR_FORMAT;
  }
  out->opcode = opcode;
  out->flags = flags;
  out->size = size;
  return ZR_OK;
}

static zr_result_t zr_dl_read_cmd_fill_rect(zr_byte_reader_t* r, zr_dl_cmd_fill_rect_t* out) {
  zr_result_t rc = ZR_OK;
  if (!out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  rc = zr_dl_read_i32le(r, &out->x);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_read_i32le(r, &out->y);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_read_i32le(r, &out->w);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_read_i32le(r, &out->h);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_read_style(r, &out->style);
  if (rc != ZR_OK) {
    return rc;
  }
  return ZR_OK;
}

static zr_result_t zr_dl_read_cmd_draw_text(zr_byte_reader_t* r, zr_dl_cmd_draw_text_t* out) {
  zr_result_t rc = ZR_OK;
  if (!out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  rc = zr_dl_read_i32le(r, &out->x);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_read_i32le(r, &out->y);
  if (rc != ZR_OK) {
    return rc;
  }
  if (!zr_byte_reader_read_u32le(r, &out->string_index) || !zr_byte_reader_read_u32le(r, &out->byte_off) ||
      !zr_byte_reader_read_u32le(r, &out->byte_len)) {
    return ZR_ERR_FORMAT;
  }
  rc = zr_dl_read_style(r, &out->style);
  if (rc != ZR_OK) {
    return rc;
  }
  if (!zr_byte_reader_read_u32le(r, &out->reserved0)) {
    return ZR_ERR_FORMAT;
  }
  return ZR_OK;
}

static zr_result_t zr_dl_read_cmd_push_clip(zr_byte_reader_t* r, zr_dl_cmd_push_clip_t* out) {
  zr_result_t rc = ZR_OK;
  if (!out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  rc = zr_dl_read_i32le(r, &out->x);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_read_i32le(r, &out->y);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_read_i32le(r, &out->w);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_read_i32le(r, &out->h);
  if (rc != ZR_OK) {
    return rc;
  }
  return ZR_OK;
}

static zr_result_t zr_dl_read_cmd_draw_text_run(zr_byte_reader_t* r, zr_dl_cmd_draw_text_run_t* out) {
  zr_result_t rc = ZR_OK;
  if (!out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  rc = zr_dl_read_i32le(r, &out->x);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_read_i32le(r, &out->y);
  if (rc != ZR_OK) {
    return rc;
  }
  if (!zr_byte_reader_read_u32le(r, &out->blob_index) || !zr_byte_reader_read_u32le(r, &out->reserved0)) {
    return ZR_ERR_FORMAT;
  }
  return ZR_OK;
}

static zr_result_t zr_dl_read_cmd_set_cursor(zr_byte_reader_t* r, zr_dl_cmd_set_cursor_t* out) {
  zr_result_t rc = ZR_OK;
  if (!out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  rc = zr_dl_read_i32le(r, &out->x);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_read_i32le(r, &out->y);
  if (rc != ZR_OK) {
    return rc;
  }

  if (!zr_byte_reader_read_u8(r, &out->shape) || !zr_byte_reader_read_u8(r, &out->visible) ||
      !zr_byte_reader_read_u8(r, &out->blink) || !zr_byte_reader_read_u8(r, &out->reserved0)) {
    return ZR_ERR_FORMAT;
  }

  return ZR_OK;
}

static zr_result_t zr_dl_read_header(const uint8_t* bytes, size_t bytes_len, zr_dl_header_t* out) {
  if (!out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  memset(out, 0, sizeof(*out));
  if (!bytes || bytes_len < sizeof(zr_dl_header_t)) {
    return ZR_ERR_FORMAT;
  }

  zr_byte_reader_t r;
  zr_byte_reader_init(&r, bytes, bytes_len);

  uint32_t fields[16];
  for (size_t i = 0u; i < 16u; i++) {
    if (!zr_byte_reader_read_u32le(&r, &fields[i])) {
      return ZR_ERR_FORMAT;
    }
  }

  out->magic = fields[0];
  out->version = fields[1];
  out->header_size = fields[2];
  out->total_size = fields[3];
  out->cmd_offset = fields[4];
  out->cmd_bytes = fields[5];
  out->cmd_count = fields[6];
  out->strings_span_offset = fields[7];
  out->strings_count = fields[8];
  out->strings_bytes_offset = fields[9];
  out->strings_bytes_len = fields[10];
  out->blobs_span_offset = fields[11];
  out->blobs_count = fields[12];
  out->blobs_bytes_offset = fields[13];
  out->blobs_bytes_len = fields[14];
  out->reserved0 = fields[15];

  return ZR_OK;
}

typedef struct zr_dl_range_t {
  uint32_t off;
  uint32_t len;
} zr_dl_range_t;

static bool zr_dl_range_is_empty(zr_dl_range_t r) {
  return r.len == 0u;
}

/* Validate that a byte range [off, off+len) fits within the buffer. */
static zr_result_t zr_dl_range_validate(zr_dl_range_t r, size_t bytes_len) {
  size_t end = 0u;
  if (!zr_checked_add_u32_to_size(r.off, r.len, &end)) {
    return ZR_ERR_FORMAT;
  }
  if (end > bytes_len) {
    return ZR_ERR_FORMAT;
  }
  return ZR_OK;
}

/* Check if two byte ranges overlap (empty ranges never overlap). */
static bool zr_dl_ranges_overlap(zr_dl_range_t a, zr_dl_range_t b) {
  if (zr_dl_range_is_empty(a) || zr_dl_range_is_empty(b)) {
    return false;
  }
  size_t a2 = 0u;
  size_t b2 = 0u;
  if (!zr_checked_add_u32_to_size(a.off, a.len, &a2) || !zr_checked_add_u32_to_size(b.off, b.len, &b2)) {
    return true;
  }
  const size_t a1 = (size_t)a.off;
  const size_t b1 = (size_t)b.off;
  return (a1 < b2) && (b1 < a2);
}

static zr_result_t zr_dl_span_read_host(const uint8_t* p, zr_dl_span_t* out) {
  if (!p || !out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  out->off = zr_load_u32le(p + 0);
  out->len = zr_load_u32le(p + 4);
  return ZR_OK;
}

static zr_result_t zr_dl_validate_text_run_blob(const zr_dl_view_t* v, uint32_t blob_index, const zr_limits_t* lim);

typedef struct zr_dl_v1_ranges_t {
  zr_dl_range_t header;
  zr_dl_range_t cmd;
  zr_dl_range_t strings_spans;
  zr_dl_range_t strings_bytes;
  zr_dl_range_t blobs_spans;
  zr_dl_range_t blobs_bytes;
} zr_dl_v1_ranges_t;

/* Validate drawlist header: magic, version, alignment, caps, and section offsets. */
static zr_result_t zr_dl_validate_header(const zr_dl_header_t* hdr, size_t bytes_len, const zr_limits_t* lim) {
  if (!hdr || !lim) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  if (hdr->magic != ZR_DL_MAGIC) {
    return ZR_ERR_FORMAT;
  }
  if (hdr->version != ZR_DRAWLIST_VERSION_V1 && hdr->version != ZR_DRAWLIST_VERSION_V2) {
    return ZR_ERR_UNSUPPORTED;
  }
  if (hdr->header_size != (uint32_t)sizeof(zr_dl_header_t)) {
    return ZR_ERR_FORMAT;
  }
  if (hdr->total_size != (uint32_t)bytes_len) {
    return ZR_ERR_FORMAT;
  }
  if (!zr_is_aligned4_u32(hdr->total_size) || !zr_is_aligned4_u32(hdr->cmd_bytes) ||
      !zr_is_aligned4_u32(hdr->strings_bytes_len) || !zr_is_aligned4_u32(hdr->blobs_bytes_len)) {
    return ZR_ERR_FORMAT;
  }

  if (hdr->reserved0 != 0u) {
    return ZR_ERR_FORMAT;
  }

  if (hdr->cmd_count > lim->dl_max_cmds || hdr->strings_count > lim->dl_max_strings ||
      hdr->blobs_count > lim->dl_max_blobs) {
    return ZR_ERR_LIMIT;
  }

  if (hdr->strings_count == 0u) {
    if (hdr->strings_span_offset != 0u || hdr->strings_bytes_offset != 0u || hdr->strings_bytes_len != 0u) {
      return ZR_ERR_FORMAT;
    }
  }
  if (hdr->blobs_count == 0u) {
    if (hdr->blobs_span_offset != 0u || hdr->blobs_bytes_offset != 0u || hdr->blobs_bytes_len != 0u) {
      return ZR_ERR_FORMAT;
    }
  }

  if (!zr_is_aligned4_u32(hdr->cmd_offset) || !zr_is_aligned4_u32(hdr->strings_span_offset) ||
      !zr_is_aligned4_u32(hdr->strings_bytes_offset) || !zr_is_aligned4_u32(hdr->blobs_span_offset) ||
      !zr_is_aligned4_u32(hdr->blobs_bytes_offset)) {
    return ZR_ERR_FORMAT;
  }

  if (hdr->cmd_count == 0u) {
    if (hdr->cmd_offset != 0u || hdr->cmd_bytes != 0u) {
      return ZR_ERR_FORMAT;
    }
  }

  return ZR_OK;
}

/* Build the set of byte ranges for each drawlist section from the header. */
static zr_result_t zr_dl_build_ranges_v1(const zr_dl_header_t* hdr, uint32_t strings_span_bytes,
                                         uint32_t blobs_span_bytes, zr_dl_v1_ranges_t* out) {
  if (!hdr || !out) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  out->header.off = 0u;
  out->header.len = (uint32_t)sizeof(zr_dl_header_t);

  out->cmd.off = hdr->cmd_offset;
  out->cmd.len = hdr->cmd_bytes;

  out->strings_spans.off = hdr->strings_span_offset;
  out->strings_spans.len = strings_span_bytes;

  out->strings_bytes.off = hdr->strings_bytes_offset;
  out->strings_bytes.len = hdr->strings_bytes_len;

  out->blobs_spans.off = hdr->blobs_span_offset;
  out->blobs_spans.len = blobs_span_bytes;

  out->blobs_bytes.off = hdr->blobs_bytes_offset;
  out->blobs_bytes.len = hdr->blobs_bytes_len;

  return ZR_OK;
}

/* Ensure all section ranges fit in buffer and none overlap with each other. */
static zr_result_t zr_dl_validate_ranges_v1(const zr_dl_v1_ranges_t* r, size_t bytes_len) {
  if (!r) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  if (zr_dl_range_validate(r->cmd, bytes_len) != ZR_OK || zr_dl_range_validate(r->strings_spans, bytes_len) != ZR_OK ||
      zr_dl_range_validate(r->strings_bytes, bytes_len) != ZR_OK ||
      zr_dl_range_validate(r->blobs_spans, bytes_len) != ZR_OK ||
      zr_dl_range_validate(r->blobs_bytes, bytes_len) != ZR_OK) {
    return ZR_ERR_FORMAT;
  }

  const zr_dl_range_t non_header[] = {r->cmd, r->strings_spans, r->strings_bytes, r->blobs_spans, r->blobs_bytes};
  for (size_t i = 0u; i < sizeof(non_header) / sizeof(non_header[0]); i++) {
    if (zr_dl_ranges_overlap(r->header, non_header[i])) {
      return ZR_ERR_FORMAT;
    }
  }
  for (size_t i = 0u; i < sizeof(non_header) / sizeof(non_header[0]); i++) {
    for (size_t j = i + 1u; j < sizeof(non_header) / sizeof(non_header[0]); j++) {
      if (zr_dl_ranges_overlap(non_header[i], non_header[j])) {
        return ZR_ERR_FORMAT;
      }
    }
  }

  return ZR_OK;
}

/* Validate that all spans in a span table fit within the payload section. */
static zr_result_t zr_dl_validate_span_table_v1(const uint8_t* bytes, uint32_t span_table_offset, uint32_t span_count,
                                                uint32_t payload_bytes_len) {
  if (!bytes) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  for (uint32_t i = 0u; i < span_count; i++) {
    const size_t span_off = (size_t)i * sizeof(zr_dl_span_t);
    zr_dl_span_t span;
    if (zr_dl_span_read_host(bytes + span_table_offset + span_off, &span) != ZR_OK) {
      return ZR_ERR_FORMAT;
    }
    size_t end = 0u;
    if (!zr_checked_add_u32_to_size(span.off, span.len, &end)) {
      return ZR_ERR_FORMAT;
    }
    if (end > (size_t)payload_bytes_len) {
      return ZR_ERR_FORMAT;
    }
  }
  return ZR_OK;
}

/* Initialize a validated view structure with pointers into the drawlist buffer. */
static void zr_dl_view_init(zr_dl_view_t* view, const zr_dl_header_t* hdr, const uint8_t* bytes, size_t bytes_len) {
  memset(view, 0, sizeof(*view));
  view->hdr = *hdr;
  view->bytes = bytes;
  view->bytes_len = bytes_len;
  view->cmd_bytes = (hdr->cmd_bytes != 0u) ? (bytes + hdr->cmd_offset) : NULL;
  view->cmd_bytes_len = (size_t)hdr->cmd_bytes;
  view->strings_span_bytes = (hdr->strings_count != 0u) ? (bytes + hdr->strings_span_offset) : NULL;
  view->strings_count = (size_t)hdr->strings_count;
  view->strings_bytes = (hdr->strings_count != 0u) ? (bytes + hdr->strings_bytes_offset) : NULL;
  view->strings_bytes_len = (size_t)hdr->strings_bytes_len;
  view->blobs_span_bytes = (hdr->blobs_count != 0u) ? (bytes + hdr->blobs_span_offset) : NULL;
  view->blobs_count = (size_t)hdr->blobs_count;
  view->blobs_bytes = (hdr->blobs_count != 0u) ? (bytes + hdr->blobs_bytes_offset) : NULL;
  view->blobs_bytes_len = (size_t)hdr->blobs_bytes_len;
}

/* Walk and validate every command in the command stream (framing, opcodes, indices). */
static zr_result_t zr_dl_validate_cmd_stream_v1(const zr_dl_view_t* view, const zr_limits_t* lim) {
  if (!view || !lim) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_byte_reader_t r;
  zr_byte_reader_init(&r, view->cmd_bytes, view->cmd_bytes_len);

  uint32_t clip_depth = 0u;

  for (uint32_t ci = 0u; ci < view->hdr.cmd_count; ci++) {
    zr_dl_cmd_header_t ch;
    zr_result_t rc = zr_dl_read_cmd_header(&r, &ch);
    if (rc != ZR_OK) {
      return rc;
    }
    if (ch.flags != 0u) {
      return ZR_ERR_FORMAT;
    }
    if (ch.size < (uint32_t)sizeof(zr_dl_cmd_header_t) || (ch.size & 3u) != 0u) {
      return ZR_ERR_FORMAT;
    }
    const size_t payload = (size_t)ch.size - sizeof(zr_dl_cmd_header_t);
    if (zr_byte_reader_remaining(&r) < payload) {
      return ZR_ERR_FORMAT;
    }

    switch ((zr_dl_opcode_t)ch.opcode) {
    case ZR_DL_OP_CLEAR: {
      if (ch.size != (uint32_t)sizeof(zr_dl_cmd_header_t)) {
        return ZR_ERR_FORMAT;
      }
      break;
    }
    case ZR_DL_OP_FILL_RECT: {
      if (ch.size != (uint32_t)(sizeof(zr_dl_cmd_header_t) + sizeof(zr_dl_cmd_fill_rect_t))) {
        return ZR_ERR_FORMAT;
      }
      zr_dl_cmd_fill_rect_t cmd;
      rc = zr_dl_read_cmd_fill_rect(&r, &cmd);
      if (rc != ZR_OK) {
        return rc;
      }
      if (cmd.style.reserved0 != 0u) {
        return ZR_ERR_FORMAT;
      }
      break;
    }
    case ZR_DL_OP_DRAW_TEXT: {
      if (ch.size != (uint32_t)(sizeof(zr_dl_cmd_header_t) + sizeof(zr_dl_cmd_draw_text_t))) {
        return ZR_ERR_FORMAT;
      }
      zr_dl_cmd_draw_text_t cmd;
      rc = zr_dl_read_cmd_draw_text(&r, &cmd);
      if (rc != ZR_OK) {
        return rc;
      }
      if (cmd.style.reserved0 != 0u || cmd.reserved0 != 0u) {
        return ZR_ERR_FORMAT;
      }
      if (cmd.string_index >= view->hdr.strings_count) {
        return ZR_ERR_FORMAT;
      }
      const size_t span_off = (size_t)cmd.string_index * sizeof(zr_dl_span_t);
      zr_dl_span_t span;
      if (zr_dl_span_read_host(view->strings_span_bytes + span_off, &span) != ZR_OK) {
        return ZR_ERR_FORMAT;
      }
      uint32_t slice_end = 0u;
      if (!zr_checked_add_u32(cmd.byte_off, cmd.byte_len, &slice_end)) {
        return ZR_ERR_FORMAT;
      }
      if (slice_end > span.len) {
        return ZR_ERR_FORMAT;
      }
      break;
    }
    case ZR_DL_OP_PUSH_CLIP: {
      if (ch.size != (uint32_t)(sizeof(zr_dl_cmd_header_t) + sizeof(zr_dl_cmd_push_clip_t))) {
        return ZR_ERR_FORMAT;
      }
      zr_dl_cmd_push_clip_t cmd;
      rc = zr_dl_read_cmd_push_clip(&r, &cmd);
      if (rc != ZR_OK) {
        return rc;
      }
      clip_depth++;
      if (clip_depth > lim->dl_max_clip_depth) {
        return ZR_ERR_LIMIT;
      }
      break;
    }
    case ZR_DL_OP_POP_CLIP: {
      if (ch.size != (uint32_t)sizeof(zr_dl_cmd_header_t)) {
        return ZR_ERR_FORMAT;
      }
      if (clip_depth == 0u) {
        return ZR_ERR_FORMAT;
      }
      clip_depth--;
      break;
    }
    case ZR_DL_OP_DRAW_TEXT_RUN: {
      if (ch.size != (uint32_t)(sizeof(zr_dl_cmd_header_t) + sizeof(zr_dl_cmd_draw_text_run_t))) {
        return ZR_ERR_FORMAT;
      }
      zr_dl_cmd_draw_text_run_t cmd;
      rc = zr_dl_read_cmd_draw_text_run(&r, &cmd);
      if (rc != ZR_OK) {
        return rc;
      }
      if (cmd.reserved0 != 0u) {
        return ZR_ERR_FORMAT;
      }
      rc = zr_dl_validate_text_run_blob(view, cmd.blob_index, lim);
      if (rc != ZR_OK) {
        return rc;
      }
      break;
    }
    default: {
      return ZR_ERR_UNSUPPORTED;
    }
    }
  }

  if (zr_byte_reader_remaining(&r) != 0u) {
    return ZR_ERR_FORMAT;
  }

  return ZR_OK;
}

/* Walk and validate every command in the command stream (v2: includes cursor op). */
static zr_result_t zr_dl_validate_cmd_stream_v2(const zr_dl_view_t* view, const zr_limits_t* lim) {
  if (!view || !lim) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_byte_reader_t r;
  zr_byte_reader_init(&r, view->cmd_bytes, view->cmd_bytes_len);

  uint32_t clip_depth = 0u;

  for (uint32_t ci = 0u; ci < view->hdr.cmd_count; ci++) {
    zr_dl_cmd_header_t ch;
    zr_result_t rc = zr_dl_read_cmd_header(&r, &ch);
    if (rc != ZR_OK) {
      return rc;
    }
    if (ch.flags != 0u) {
      return ZR_ERR_FORMAT;
    }
    if (ch.size < (uint32_t)sizeof(zr_dl_cmd_header_t) || (ch.size & 3u) != 0u) {
      return ZR_ERR_FORMAT;
    }
    const size_t payload = (size_t)ch.size - sizeof(zr_dl_cmd_header_t);
    if (zr_byte_reader_remaining(&r) < payload) {
      return ZR_ERR_FORMAT;
    }

    switch ((zr_dl_opcode_t)ch.opcode) {
    case ZR_DL_OP_CLEAR: {
      if (ch.size != (uint32_t)sizeof(zr_dl_cmd_header_t)) {
        return ZR_ERR_FORMAT;
      }
      break;
    }
    case ZR_DL_OP_FILL_RECT: {
      if (ch.size != (uint32_t)(sizeof(zr_dl_cmd_header_t) + sizeof(zr_dl_cmd_fill_rect_t))) {
        return ZR_ERR_FORMAT;
      }
      zr_dl_cmd_fill_rect_t cmd;
      rc = zr_dl_read_cmd_fill_rect(&r, &cmd);
      if (rc != ZR_OK) {
        return rc;
      }
      if (cmd.style.reserved0 != 0u) {
        return ZR_ERR_FORMAT;
      }
      break;
    }
    case ZR_DL_OP_DRAW_TEXT: {
      if (ch.size != (uint32_t)(sizeof(zr_dl_cmd_header_t) + sizeof(zr_dl_cmd_draw_text_t))) {
        return ZR_ERR_FORMAT;
      }
      zr_dl_cmd_draw_text_t cmd;
      rc = zr_dl_read_cmd_draw_text(&r, &cmd);
      if (rc != ZR_OK) {
        return rc;
      }
      if (cmd.style.reserved0 != 0u || cmd.reserved0 != 0u) {
        return ZR_ERR_FORMAT;
      }
      if (cmd.string_index >= view->hdr.strings_count) {
        return ZR_ERR_FORMAT;
      }
      const size_t span_off = (size_t)cmd.string_index * sizeof(zr_dl_span_t);
      zr_dl_span_t span;
      if (zr_dl_span_read_host(view->strings_span_bytes + span_off, &span) != ZR_OK) {
        return ZR_ERR_FORMAT;
      }
      uint32_t slice_end = 0u;
      if (!zr_checked_add_u32(cmd.byte_off, cmd.byte_len, &slice_end)) {
        return ZR_ERR_FORMAT;
      }
      if (slice_end > span.len) {
        return ZR_ERR_FORMAT;
      }
      break;
    }
    case ZR_DL_OP_PUSH_CLIP: {
      if (ch.size != (uint32_t)(sizeof(zr_dl_cmd_header_t) + sizeof(zr_dl_cmd_push_clip_t))) {
        return ZR_ERR_FORMAT;
      }
      zr_dl_cmd_push_clip_t cmd;
      rc = zr_dl_read_cmd_push_clip(&r, &cmd);
      if (rc != ZR_OK) {
        return rc;
      }
      clip_depth++;
      if (clip_depth > lim->dl_max_clip_depth) {
        return ZR_ERR_LIMIT;
      }
      break;
    }
    case ZR_DL_OP_POP_CLIP: {
      if (ch.size != (uint32_t)sizeof(zr_dl_cmd_header_t)) {
        return ZR_ERR_FORMAT;
      }
      if (clip_depth == 0u) {
        return ZR_ERR_FORMAT;
      }
      clip_depth--;
      break;
    }
    case ZR_DL_OP_DRAW_TEXT_RUN: {
      if (ch.size != (uint32_t)(sizeof(zr_dl_cmd_header_t) + sizeof(zr_dl_cmd_draw_text_run_t))) {
        return ZR_ERR_FORMAT;
      }
      zr_dl_cmd_draw_text_run_t cmd;
      rc = zr_dl_read_cmd_draw_text_run(&r, &cmd);
      if (rc != ZR_OK) {
        return rc;
      }
      if (cmd.reserved0 != 0u) {
        return ZR_ERR_FORMAT;
      }
      rc = zr_dl_validate_text_run_blob(view, cmd.blob_index, lim);
      if (rc != ZR_OK) {
        return rc;
      }
      break;
    }
    case ZR_DL_OP_SET_CURSOR: {
      if (ch.size != (uint32_t)(sizeof(zr_dl_cmd_header_t) + sizeof(zr_dl_cmd_set_cursor_t))) {
        return ZR_ERR_FORMAT;
      }
      zr_dl_cmd_set_cursor_t cmd;
      rc = zr_dl_read_cmd_set_cursor(&r, &cmd);
      if (rc != ZR_OK) {
        return rc;
      }
      if (cmd.reserved0 != 0u) {
        return ZR_ERR_FORMAT;
      }
      if (cmd.x < -1 || cmd.y < -1) {
        return ZR_ERR_FORMAT;
      }
      if (cmd.shape > ZR_CURSOR_SHAPE_BAR) {
        return ZR_ERR_FORMAT;
      }
      if (cmd.visible > 1u || cmd.blink > 1u) {
        return ZR_ERR_FORMAT;
      }
      break;
    }
    default: {
      return ZR_ERR_UNSUPPORTED;
    }
    }
  }

  if (zr_byte_reader_remaining(&r) != 0u) {
    return ZR_ERR_FORMAT;
  }

  return ZR_OK;
}

/* Validate a text run blob: segment count, alignment, and all string references. */
static zr_result_t zr_dl_validate_text_run_blob(const zr_dl_view_t* v, uint32_t blob_index, const zr_limits_t* lim) {
  if (!v || !lim) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (blob_index >= v->blobs_count) {
    return ZR_ERR_FORMAT;
  }

  const size_t span_off = (size_t)blob_index * sizeof(zr_dl_span_t);
  zr_dl_span_t span;
  if (zr_dl_span_read_host(v->blobs_span_bytes + span_off, &span) != ZR_OK) {
    return ZR_ERR_FORMAT;
  }

  if (!zr_is_aligned4_u32(span.off) || (span.len & 3u) != 0u) {
    return ZR_ERR_FORMAT;
  }
  if (span.off > (uint32_t)v->blobs_bytes_len) {
    return ZR_ERR_FORMAT;
  }
  size_t end = 0u;
  if (!zr_checked_add_u32_to_size(span.off, span.len, &end)) {
    return ZR_ERR_FORMAT;
  }
  if (end > v->blobs_bytes_len) {
    return ZR_ERR_FORMAT;
  }

  const uint8_t* blob = v->blobs_bytes + span.off;
  zr_byte_reader_t r;
  zr_byte_reader_init(&r, blob, (size_t)span.len);

  uint32_t seg_count = 0u;
  if (!zr_byte_reader_read_u32le(&r, &seg_count)) {
    return ZR_ERR_FORMAT;
  }
  if (seg_count > lim->dl_max_text_run_segments) {
    return ZR_ERR_LIMIT;
  }

  const size_t seg_size = sizeof(zr_dl_style_t) + 12u;
  size_t expected = 0u;
  if (!zr_checked_mul_size((size_t)seg_count, seg_size, &expected)) {
    return ZR_ERR_FORMAT;
  }
  if (!zr_checked_add_size(expected, 4u, &expected)) {
    return ZR_ERR_FORMAT;
  }
  if (expected != (size_t)span.len) {
    return ZR_ERR_FORMAT;
  }

  for (uint32_t i = 0u; i < seg_count; i++) {
    zr_dl_style_t style;
    if (zr_dl_read_style(&r, &style) != ZR_OK) {
      return ZR_ERR_FORMAT;
    }
    if (style.reserved0 != 0u) {
      return ZR_ERR_FORMAT;
    }
    uint32_t string_index = 0u;
    uint32_t byte_off = 0u;
    uint32_t byte_len = 0u;
    if (!zr_byte_reader_read_u32le(&r, &string_index) || !zr_byte_reader_read_u32le(&r, &byte_off) ||
        !zr_byte_reader_read_u32le(&r, &byte_len)) {
      return ZR_ERR_FORMAT;
    }
    if (string_index >= v->strings_count) {
      return ZR_ERR_FORMAT;
    }
    const size_t str_span_off = (size_t)string_index * sizeof(zr_dl_span_t);
    zr_dl_span_t str_span;
    if (zr_dl_span_read_host(v->strings_span_bytes + str_span_off, &str_span) != ZR_OK) {
      return ZR_ERR_FORMAT;
    }
    uint32_t slice_end = 0u;
    if (!zr_checked_add_u32(byte_off, byte_len, &slice_end)) {
      return ZR_ERR_FORMAT;
    }
    if (slice_end > str_span.len) {
      return ZR_ERR_FORMAT;
    }
  }
  return ZR_OK;
}

/* Fully validate a drawlist buffer and produce a view for execution.
 * Checks header, section ranges, span tables, and all command stream contents. */
zr_result_t zr_dl_validate(const uint8_t* bytes, size_t bytes_len, const zr_limits_t* lim, zr_dl_view_t* out_view) {
  if (!bytes || !lim || !out_view) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  memset(out_view, 0, sizeof(*out_view));

  if (bytes_len > (size_t)lim->dl_max_total_bytes) {
    return ZR_ERR_LIMIT;
  }

  zr_dl_header_t hdr;
  zr_result_t rc = zr_dl_read_header(bytes, bytes_len, &hdr);
  if (rc != ZR_OK) {
    return rc;
  }

  rc = zr_dl_validate_header(&hdr, bytes_len, lim);
  if (rc != ZR_OK) {
    return rc;
  }

  uint32_t str_span_bytes = 0u;
  if (!zr_checked_mul_u32_to_u32(hdr.strings_count, (uint32_t)sizeof(zr_dl_span_t), &str_span_bytes)) {
    return ZR_ERR_FORMAT;
  }
  uint32_t blob_span_bytes = 0u;
  if (!zr_checked_mul_u32_to_u32(hdr.blobs_count, (uint32_t)sizeof(zr_dl_span_t), &blob_span_bytes)) {
    return ZR_ERR_FORMAT;
  }

  zr_dl_v1_ranges_t ranges;
  rc = zr_dl_build_ranges_v1(&hdr, str_span_bytes, blob_span_bytes, &ranges);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_validate_ranges_v1(&ranges, bytes_len);
  if (rc != ZR_OK) {
    return rc;
  }

  /* Span tables. */
  rc = zr_dl_validate_span_table_v1(bytes, hdr.strings_span_offset, hdr.strings_count, hdr.strings_bytes_len);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_dl_validate_span_table_v1(bytes, hdr.blobs_span_offset, hdr.blobs_count, hdr.blobs_bytes_len);
  if (rc != ZR_OK) {
    return rc;
  }

  /* Command stream framing + opcode validation. */
  zr_dl_view_t view;
  zr_dl_view_init(&view, &hdr, bytes, bytes_len);
  if (hdr.version == ZR_DRAWLIST_VERSION_V1) {
    rc = zr_dl_validate_cmd_stream_v1(&view, lim);
  } else if (hdr.version == ZR_DRAWLIST_VERSION_V2) {
    rc = zr_dl_validate_cmd_stream_v2(&view, lim);
  } else {
    rc = ZR_ERR_UNSUPPORTED;
  }
  if (rc != ZR_OK) {
    return rc;
  }

  *out_view = view;
  return ZR_OK;
}

static zr_style_t zr_style_from_dl(zr_dl_style_t s) {
  zr_style_t out;
  out.fg_rgb = s.fg;
  out.bg_rgb = s.bg;
  out.attrs = s.attrs;
  out.reserved = s.reserved0;
  return out;
}

static zr_result_t zr_dl_exec_clear(zr_fb_t* dst) {
  return zr_fb_clear(dst, NULL);
}

static bool zr_dl_is_tab_grapheme(const uint8_t* bytes, size_t len) {
  zr_utf8_decode_result_t d = zr_utf8_decode_one(bytes, len);
  return d.valid != 0u && d.scalar == 0x09u;
}

static uint32_t zr_dl_tab_advance(int32_t col, uint32_t tab_width) {
  const uint32_t safe_col = (col <= 0) ? 0u : (uint32_t)col;
  const uint32_t rem = safe_col % tab_width;
  return (rem == 0u) ? tab_width : (tab_width - rem);
}

static zr_result_t zr_dl_draw_tab_spaces(zr_fb_painter_t* p, int32_t y, int32_t* inout_x, uint32_t tab_width,
                                         const zr_style_t* style) {
  if (!p || !inout_x || tab_width == 0u || !style) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  const uint8_t space = (uint8_t)' ';
  int32_t cx = *inout_x;
  const uint32_t adv = zr_dl_tab_advance(cx, tab_width);
  for (uint32_t i = 0u; i < adv; i++) {
    (void)zr_fb_put_grapheme(p, cx, y, &space, 1u, 1u, style);
    if (cx > (INT32_MAX - 1)) {
      return ZR_ERR_LIMIT;
    }
    cx += 1;
  }

  *inout_x = cx;
  return ZR_OK;
}

/*
 * Draw UTF-8 bytes into the framebuffer by grapheme iteration.
 *
 * Why: The framebuffer primitive is zr_fb_put_grapheme (already segmented,
 * width provided). Drawlist execution owns segmentation and deterministic width.
 */
static zr_result_t zr_dl_draw_text_utf8(zr_fb_painter_t* p, int32_t y, int32_t* inout_x, const uint8_t* bytes,
                                        size_t len, uint32_t tab_width, uint32_t width_policy,
                                        const zr_style_t* style) {
  if (!p || !inout_x || !bytes || !style || tab_width == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  int32_t cx = *inout_x;
  zr_grapheme_iter_t it;
  zr_grapheme_iter_init(&it, bytes, len);

  zr_grapheme_t g;
  while (zr_grapheme_next(&it, &g)) {
    const uint8_t* gb = bytes + g.offset;
    const size_t gl = g.size;

    /* --- Tab expansion (policy: spaces to the next tab stop) --- */
    if (zr_dl_is_tab_grapheme(gb, gl)) {
      zr_result_t rc = zr_dl_draw_tab_spaces(p, y, &cx, tab_width, style);
      if (rc != ZR_OK) {
        return rc;
      }
      continue;
    }

    /* --- Grapheme width and write --- */
    const uint8_t w = zr_width_grapheme_utf8(gb, gl, (zr_width_policy_t)width_policy);
    if (w == 0u) {
      continue;
    }

    /*
      Important: cursor advancement must not depend on clipping. The framebuffer
      primitive handles "no half glyph" replacement internally; drawlist text
      maintains logical positions by always advancing by the original width.
    */
    (void)zr_fb_put_grapheme(p, cx, y, gb, gl, w, style);
    if (cx > (INT32_MAX - (int32_t)w)) {
      return ZR_ERR_LIMIT;
    }
    cx += (int32_t)w;
  }

  *inout_x = cx;
  return ZR_OK;
}

static zr_result_t zr_dl_exec_fill_rect(zr_byte_reader_t* r, zr_fb_painter_t* p) {
  zr_dl_cmd_fill_rect_t cmd;
  zr_result_t rc = zr_dl_read_cmd_fill_rect(r, &cmd);
  if (rc != ZR_OK) {
    return rc;
  }
  zr_rect_t rr = {cmd.x, cmd.y, cmd.w, cmd.h};
  zr_style_t s = zr_style_from_dl(cmd.style);
  return zr_fb_fill_rect(p, rr, &s);
}

static zr_result_t zr_dl_exec_draw_text(zr_byte_reader_t* r, const zr_dl_view_t* v, zr_fb_painter_t* p) {
  zr_dl_cmd_draw_text_t cmd;
  zr_result_t rc = zr_dl_read_cmd_draw_text(r, &cmd);
  if (rc != ZR_OK) {
    return rc;
  }

  const size_t span_off = (size_t)cmd.string_index * sizeof(zr_dl_span_t);
  zr_dl_span_t sspan;
  if (zr_dl_span_read_host(v->strings_span_bytes + span_off, &sspan) != ZR_OK) {
    return ZR_ERR_FORMAT;
  }

  const uint8_t* sbytes = v->strings_bytes + sspan.off + cmd.byte_off;
  zr_style_t s = zr_style_from_dl(cmd.style);
  int32_t cx = cmd.x;
  return zr_dl_draw_text_utf8(p, cmd.y, &cx, sbytes, (size_t)cmd.byte_len, v->text.tab_width, v->text.width_policy, &s);
}

static zr_result_t zr_dl_exec_push_clip(zr_byte_reader_t* r, zr_fb_painter_t* p) {
  zr_dl_cmd_push_clip_t cmd;
  zr_result_t rc = zr_dl_read_cmd_push_clip(r, &cmd);
  if (rc != ZR_OK) {
    return rc;
  }
  zr_rect_t next = {cmd.x, cmd.y, cmd.w, cmd.h};
  return zr_fb_clip_push(p, next);
}

static zr_result_t zr_dl_exec_pop_clip(zr_fb_painter_t* p) {
  zr_result_t rc = zr_fb_clip_pop(p);
  if (rc == ZR_ERR_LIMIT) {
    return ZR_ERR_FORMAT;
  }
  return rc;
}

static zr_result_t zr_dl_exec_draw_text_run_segment(const zr_dl_view_t* v, zr_byte_reader_t* br, zr_fb_painter_t* p,
                                                    int32_t y, int32_t* inout_x) {
  /*
    Note: This path assumes `v` came from zr_dl_validate() (so all indices and
    spans are in-bounds). Execution is structured as a straight-line interpreter
    for readability.
  */
  zr_dl_style_t style;
  if (zr_dl_read_style(br, &style) != ZR_OK) {
    return ZR_ERR_FORMAT;
  }

  uint32_t string_index = 0u;
  uint32_t byte_off = 0u;
  uint32_t byte_len = 0u;
  if (!zr_byte_reader_read_u32le(br, &string_index) || !zr_byte_reader_read_u32le(br, &byte_off) ||
      !zr_byte_reader_read_u32le(br, &byte_len)) {
    return ZR_ERR_FORMAT;
  }

  const size_t sspan_off = (size_t)string_index * sizeof(zr_dl_span_t);
  zr_dl_span_t sspan;
  if (zr_dl_span_read_host(v->strings_span_bytes + sspan_off, &sspan) != ZR_OK) {
    return ZR_ERR_FORMAT;
  }

  const uint8_t* sbytes = v->strings_bytes + sspan.off + byte_off;
  zr_style_t s = zr_style_from_dl(style);

  return zr_dl_draw_text_utf8(p, y, inout_x, sbytes, (size_t)byte_len, v->text.tab_width, v->text.width_policy, &s);
}

static zr_result_t zr_dl_exec_draw_text_run(zr_byte_reader_t* r, const zr_dl_view_t* v, zr_fb_painter_t* p) {
  zr_dl_cmd_draw_text_run_t cmd;
  zr_result_t rc = zr_dl_read_cmd_draw_text_run(r, &cmd);
  if (rc != ZR_OK) {
    return rc;
  }

  const size_t bspan_off = (size_t)cmd.blob_index * sizeof(zr_dl_span_t);
  zr_dl_span_t bspan;
  if (zr_dl_span_read_host(v->blobs_span_bytes + bspan_off, &bspan) != ZR_OK) {
    return ZR_ERR_FORMAT;
  }

  const uint8_t* blob = v->blobs_bytes + bspan.off;
  zr_byte_reader_t br;
  zr_byte_reader_init(&br, blob, (size_t)bspan.len);

  uint32_t seg_count = 0u;
  if (!zr_byte_reader_read_u32le(&br, &seg_count)) {
    return ZR_ERR_FORMAT;
  }

  int32_t cx = cmd.x;
  for (uint32_t si = 0u; si < seg_count; si++) {
    rc = zr_dl_exec_draw_text_run_segment(v, &br, p, cmd.y, &cx);
    if (rc != ZR_OK) {
      return rc;
    }
  }

  return ZR_OK;
}

static zr_result_t zr_dl_exec_set_cursor(zr_byte_reader_t* r, zr_cursor_state_t* inout_cursor_state) {
  if (!r || !inout_cursor_state) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_dl_cmd_set_cursor_t cmd;
  zr_result_t rc = zr_dl_read_cmd_set_cursor(r, &cmd);
  if (rc != ZR_OK) {
    return rc;
  }

  /* Assumes validation has enforced enum/boolean/rsvd constraints. */
  zr_cursor_state_t s;
  s.x = cmd.x;
  s.y = cmd.y;
  s.shape = cmd.shape;
  s.visible = cmd.visible;
  s.blink = cmd.blink;
  s.reserved0 = 0u;
  *inout_cursor_state = s;

  return ZR_OK;
}

/* Execute a validated drawlist into the framebuffer; assumes view came from zr_dl_validate. */
zr_result_t zr_dl_execute(const zr_dl_view_t* v, zr_fb_t* dst, const zr_limits_t* lim, uint32_t tab_width,
                          uint32_t width_policy, zr_cursor_state_t* inout_cursor_state) {
  if (!v || !dst || !lim) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!inout_cursor_state) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (tab_width == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (width_policy != (uint32_t)ZR_WIDTH_EMOJI_NARROW && width_policy != (uint32_t)ZR_WIDTH_EMOJI_WIDE) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  zr_dl_view_t view = *v;
  view.text.tab_width = tab_width;
  view.text.width_policy = width_policy;

  enum { kMaxClip = 64 };
  if (lim->dl_max_clip_depth > kMaxClip) {
    return ZR_ERR_LIMIT;
  }

  zr_rect_t clip_stack[kMaxClip + 1];
  zr_fb_painter_t painter;
  zr_result_t prc = zr_fb_painter_begin(&painter, dst, clip_stack, lim->dl_max_clip_depth + 1u);
  if (prc != ZR_OK) {
    return prc;
  }

  zr_byte_reader_t r;
  zr_byte_reader_init(&r, view.cmd_bytes, view.cmd_bytes_len);

  for (uint32_t ci = 0u; ci < view.hdr.cmd_count; ci++) {
    zr_dl_cmd_header_t ch;
    zr_result_t rc = zr_dl_read_cmd_header(&r, &ch);
    if (rc != ZR_OK) {
      return rc;
    }

    /* Assumes `v` came from zr_dl_validate(): framing/sizes/flags are already verified. */
    switch ((zr_dl_opcode_t)ch.opcode) {
    case ZR_DL_OP_CLEAR: {
      rc = zr_dl_exec_clear(dst);
      if (rc != ZR_OK) {
        return rc;
      }
      break;
    }
    case ZR_DL_OP_FILL_RECT: {
      rc = zr_dl_exec_fill_rect(&r, &painter);
      if (rc != ZR_OK) {
        return rc;
      }
      break;
    }
    case ZR_DL_OP_DRAW_TEXT: {
      rc = zr_dl_exec_draw_text(&r, &view, &painter);
      if (rc != ZR_OK) {
        return rc;
      }
      break;
    }
    case ZR_DL_OP_PUSH_CLIP: {
      rc = zr_dl_exec_push_clip(&r, &painter);
      if (rc != ZR_OK) {
        return rc;
      }
      break;
    }
    case ZR_DL_OP_POP_CLIP: {
      rc = zr_dl_exec_pop_clip(&painter);
      if (rc != ZR_OK) {
        return rc;
      }
      break;
    }
    case ZR_DL_OP_DRAW_TEXT_RUN: {
      rc = zr_dl_exec_draw_text_run(&r, &view, &painter);
      if (rc != ZR_OK) {
        return rc;
      }
      break;
    }
    case ZR_DL_OP_SET_CURSOR: {
      if (view.hdr.version < ZR_DRAWLIST_VERSION_V2) {
        return ZR_ERR_UNSUPPORTED;
      }
      rc = zr_dl_exec_set_cursor(&r, inout_cursor_state);
      if (rc != ZR_OK) {
        return rc;
      }
      break;
    }
    default: {
      return ZR_ERR_UNSUPPORTED;
    }
    }
  }

  return ZR_OK;
}
