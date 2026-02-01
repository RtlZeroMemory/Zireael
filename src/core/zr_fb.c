/*
  src/core/zr_fb.c — Minimal in-memory framebuffer model implementation.
*/

#include "core/zr_fb.h"

#include "util/zr_checked.h"

#include <string.h>

static zr_style_t zr_style_default(void) {
  zr_style_t s;
  s.fg = 0u;
  s.bg = 0u;
  s.attrs = 0u;
  return s;
}

zr_result_t zr_fb_init(zr_fb_t* fb, zr_fb_cell_t* backing, uint32_t cols, uint32_t rows) {
  if (!fb || (cols != 0u && rows != 0u && !backing)) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  fb->cols = cols;
  fb->rows = rows;
  fb->cells = (cols != 0u && rows != 0u) ? backing : NULL;
  return ZR_OK;
}

zr_fb_rect_i32_t zr_fb_full_clip(const zr_fb_t* fb) {
  zr_fb_rect_i32_t r;
  r.x = 0;
  r.y = 0;
  r.w = fb ? (int32_t)fb->cols : 0;
  r.h = fb ? (int32_t)fb->rows : 0;
  return r;
}

static int32_t zr_i32_max(int32_t a, int32_t b) { return (a > b) ? a : b; }
static int32_t zr_i32_min(int32_t a, int32_t b) { return (a < b) ? a : b; }

zr_fb_rect_i32_t zr_fb_clip_intersect(zr_fb_rect_i32_t a, zr_fb_rect_i32_t b) {
  const int32_t ax2 = (a.w > 0) ? (a.x + a.w) : a.x;
  const int32_t ay2 = (a.h > 0) ? (a.y + a.h) : a.y;
  const int32_t bx2 = (b.w > 0) ? (b.x + b.w) : b.x;
  const int32_t by2 = (b.h > 0) ? (b.y + b.h) : b.y;

  zr_fb_rect_i32_t r;
  const int32_t x1 = zr_i32_max(a.x, b.x);
  const int32_t y1 = zr_i32_max(a.y, b.y);
  const int32_t x2 = zr_i32_min(ax2, bx2);
  const int32_t y2 = zr_i32_min(ay2, by2);
  r.x = x1;
  r.y = y1;
  r.w = x2 - x1;
  r.h = y2 - y1;
  if (r.w < 0) {
    r.w = 0;
  }
  if (r.h < 0) {
    r.h = 0;
  }
  return r;
}

static bool zr_fb_in_clip(int32_t x, int32_t y, zr_fb_rect_i32_t clip) {
  if (clip.w <= 0 || clip.h <= 0) {
    return false;
  }
  if (x < clip.x || y < clip.y) {
    return false;
  }
  if (x >= (clip.x + clip.w) || y >= (clip.y + clip.h)) {
    return false;
  }
  return true;
}

zr_fb_cell_t* zr_fb_cell_at(zr_fb_t* fb, uint32_t x, uint32_t y) {
  if (!fb || !fb->cells || fb->cols == 0u || fb->rows == 0u) {
    return NULL;
  }
  if (x >= fb->cols || y >= fb->rows) {
    return NULL;
  }
  size_t idx = 0u;
  if (!zr_checked_mul_size((size_t)y, (size_t)fb->cols, &idx)) {
    return NULL;
  }
  if (!zr_checked_add_size(idx, (size_t)x, &idx)) {
    return NULL;
  }
  return &fb->cells[idx];
}

const zr_fb_cell_t* zr_fb_cell_at_const(const zr_fb_t* fb, uint32_t x, uint32_t y) {
  return zr_fb_cell_at((zr_fb_t*)fb, x, y);
}

zr_result_t zr_fb_clear(zr_fb_t* fb, const zr_style_t* style) {
  if (!fb) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!fb->cells || fb->cols == 0u || fb->rows == 0u) {
    return ZR_OK;
  }
  size_t total = 0u;
  if (!zr_checked_mul_size((size_t)fb->cols, (size_t)fb->rows, &total)) {
    return ZR_ERR_LIMIT;
  }
  const zr_style_t s = style ? *style : zr_style_default();
  for (size_t i = 0u; i < total; i++) {
    zr_fb_cell_t* c = &fb->cells[i];
    memset(c->glyph, 0, sizeof(c->glyph));
    c->glyph[0] = (uint8_t)' ';
    c->glyph_len = 1u;
    c->flags = 0u;
    c->style = s;
  }
  return ZR_OK;
}

zr_result_t zr_fb_fill_rect(zr_fb_t* fb, zr_fb_rect_i32_t r, const zr_style_t* style,
                            zr_fb_rect_i32_t clip) {
  if (!fb || !style) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (r.w <= 0 || r.h <= 0) {
    return ZR_OK;
  }
  if (!fb->cells || fb->cols == 0u || fb->rows == 0u) {
    return ZR_OK;
  }
  zr_fb_rect_i32_t full = zr_fb_full_clip(fb);
  clip = zr_fb_clip_intersect(clip, full);
  zr_fb_rect_i32_t rr = zr_fb_clip_intersect(r, full);

  for (int32_t yy = rr.y; yy < (rr.y + rr.h); yy++) {
    for (int32_t xx = rr.x; xx < (rr.x + rr.w); xx++) {
      if (!zr_fb_in_clip(xx, yy, clip)) {
        continue;
      }
      zr_fb_cell_t* c = zr_fb_cell_at(fb, (uint32_t)xx, (uint32_t)yy);
      if (!c) {
        continue;
      }
      memset(c->glyph, 0, sizeof(c->glyph));
      c->glyph[0] = (uint8_t)' ';
      c->glyph_len = 1u;
      c->flags = 0u;
      c->style = *style;
    }
  }
  return ZR_OK;
}

zr_result_t zr_fb_draw_text_bytes(zr_fb_t* fb, int32_t x, int32_t y, const uint8_t* bytes,
                                  size_t len, const zr_style_t* style, zr_fb_rect_i32_t clip) {
  if (!fb || !bytes || !style) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!fb->cells || fb->cols == 0u || fb->rows == 0u) {
    return ZR_OK;
  }
  zr_fb_rect_i32_t full = zr_fb_full_clip(fb);
  clip = zr_fb_clip_intersect(clip, full);

  int32_t cx = x;
  for (size_t i = 0u; i < len; i++) {
    if (cx < 0 || y < 0) {
      cx++;
      continue;
    }
    if (cx >= (int32_t)fb->cols || y >= (int32_t)fb->rows) {
      cx++;
      continue;
    }
    if (!zr_fb_in_clip(cx, y, clip)) {
      cx++;
      continue;
    }
    zr_fb_cell_t* c = zr_fb_cell_at(fb, (uint32_t)cx, (uint32_t)y);
    if (c) {
      memset(c->glyph, 0, sizeof(c->glyph));
      c->glyph[0] = bytes[i];
      c->glyph_len = 1u;
      c->flags = 0u;
      c->style = *style;
    }
    cx++;
  }
  return ZR_OK;
}

