/*
  src/core/zr_drawlist.h — Drawlist v1 ABI structs and validation/execution API.

  Why: Defines the versioned, little-endian drawlist command stream used to
  deterministically update an in-memory framebuffer with strict bounds checks.
*/

#ifndef ZR_CORE_ZR_DRAWLIST_H_INCLUDED
#define ZR_CORE_ZR_DRAWLIST_H_INCLUDED

#include "util/zr_caps.h"
#include "util/zr_result.h"

#include <stddef.h>
#include <stdint.h>

typedef struct zr_fb_t zr_fb_t;

/* ABI-facing types (little-endian on-wire). */
typedef struct zr_dl_header_t {
  uint32_t magic;
  uint32_t version;
  uint32_t header_size;
  uint32_t total_size;

  uint32_t cmd_offset;
  uint32_t cmd_bytes;
  uint32_t cmd_count;

  uint32_t strings_span_offset;
  uint32_t strings_count;
  uint32_t strings_bytes_offset;
  uint32_t strings_bytes_len;

  uint32_t blobs_span_offset;
  uint32_t blobs_count;
  uint32_t blobs_bytes_offset;
  uint32_t blobs_bytes_len;

  uint32_t reserved0;
} zr_dl_header_t;

typedef struct zr_dl_span_t {
  uint32_t off;
  uint32_t len;
} zr_dl_span_t;

typedef struct zr_dl_cmd_header_t {
  uint16_t opcode;
  uint16_t flags;
  uint32_t size;
} zr_dl_cmd_header_t;

typedef enum zr_dl_opcode_t {
  ZR_DL_OP_INVALID = 0,
  ZR_DL_OP_CLEAR = 1,
  ZR_DL_OP_FILL_RECT = 2,
  ZR_DL_OP_DRAW_TEXT = 3,
  ZR_DL_OP_PUSH_CLIP = 4,
  ZR_DL_OP_POP_CLIP = 5,
  ZR_DL_OP_DRAW_TEXT_RUN = 6
} zr_dl_opcode_t;

typedef struct zr_dl_style_t {
  uint32_t fg;
  uint32_t bg;
  uint32_t attrs;
  uint32_t reserved0; /* must be 0 in v1 */
} zr_dl_style_t;

typedef struct zr_dl_cmd_fill_rect_t {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  zr_dl_style_t style;
} zr_dl_cmd_fill_rect_t;

typedef struct zr_dl_cmd_draw_text_t {
  int32_t x;
  int32_t y;
  uint32_t string_index;
  uint32_t byte_off;
  uint32_t byte_len;
  zr_dl_style_t style;
  uint32_t reserved0; /* must be 0 in v1 */
} zr_dl_cmd_draw_text_t;

typedef struct zr_dl_cmd_push_clip_t {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
} zr_dl_cmd_push_clip_t;

typedef struct zr_dl_cmd_draw_text_run_t {
  int32_t x;
  int32_t y;
  uint32_t blob_index;
  uint32_t reserved0; /* must be 0 in v1 */
} zr_dl_cmd_draw_text_run_t;

/* Engine-internal validated view. */
typedef struct zr_dl_view_t {
  zr_dl_header_t hdr; /* host-endian copy */

  const uint8_t* bytes;
  size_t bytes_len;

  const uint8_t* cmd_bytes;
  size_t cmd_bytes_len;

  const uint8_t* strings_span_bytes;
  size_t strings_count;
  const uint8_t* strings_bytes;
  size_t strings_bytes_len;

  const uint8_t* blobs_span_bytes;
  size_t blobs_count;
  const uint8_t* blobs_bytes;
  size_t blobs_bytes_len;
} zr_dl_view_t;

zr_result_t zr_dl_validate(const uint8_t* bytes, size_t bytes_len, const zr_limits_t* lim,
                           zr_dl_view_t* out_view);
zr_result_t zr_dl_execute(const zr_dl_view_t* v, zr_fb_t* dst, const zr_limits_t* lim);

#endif /* ZR_CORE_ZR_DRAWLIST_H_INCLUDED */

