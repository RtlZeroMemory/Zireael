/*
  include/zr/zr_drawlist.h — Drawlist ABI structs (v1 + v2).

  Why: Defines the versioned, little-endian drawlist command stream used by
  wrappers to drive rendering through engine_submit_drawlist(). v1 remains
  supported and behavior-stable; v2 adds new opcodes without changing v1
  layouts.
*/

#ifndef ZR_ZR_DRAWLIST_H_INCLUDED
#define ZR_ZR_DRAWLIST_H_INCLUDED

#include <stdint.h>

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
  ZR_DL_OP_DRAW_TEXT_RUN = 6,

  /* v2: cursor control (does not draw glyphs into the framebuffer). */
  ZR_DL_OP_SET_CURSOR = 7
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

typedef struct zr_dl_cmd_set_cursor_t {
  int32_t x; /* 0-based cell; -1 means "leave unchanged" */
  int32_t y; /* 0-based cell; -1 means "leave unchanged" */
  uint8_t shape;   /* 0=block, 1=underline, 2=bar */
  uint8_t visible; /* 0/1 */
  uint8_t blink;   /* 0/1 */
  uint8_t reserved0; /* must be 0 */
} zr_dl_cmd_set_cursor_t;

#endif /* ZR_ZR_DRAWLIST_H_INCLUDED */
