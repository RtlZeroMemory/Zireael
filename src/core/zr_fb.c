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

static size_t zr_utf8_decode_one(const uint8_t* s, size_t len, uint8_t out[4], uint8_t* out_len) {
  if (!s || !out || !out_len || len == 0u) {
    return 0u;
  }

  const uint8_t b0 = s[0];
  if (b0 < 0x80u) {
    out[0] = b0;
    *out_len = 1u;
    return 1u;
  }

  /* 2-byte */
  if ((b0 & 0xE0u) == 0xC0u) {
    if (len < 2u) {
      goto invalid;
    }
    const uint8_t b1 = s[1];
    if ((b1 & 0xC0u) != 0x80u) {
      goto invalid;
    }
    /* Reject overlong encodings (U+0000..U+007F). */
    if (b0 < 0xC2u) {
      goto invalid;
    }
    out[0] = b0;
    out[1] = b1;
    *out_len = 2u;
    return 2u;
  }

  /* 3-byte */
  if ((b0 & 0xF0u) == 0xE0u) {
    if (len < 3u) {
      goto invalid;
    }
    const uint8_t b1 = s[1];
    const uint8_t b2 = s[2];
    if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u) {
      goto invalid;
    }
    if (b0 == 0xE0u && b1 < 0xA0u) {
      goto invalid;
    }
    /* UTF-16 surrogate halves */
    if (b0 == 0xEDu && b1 >= 0xA0u) {
      goto invalid;
    }
    out[0] = b0;
    out[1] = b1;
    out[2] = b2;
    *out_len = 3u;
    return 3u;
  }

  /* 4-byte */
  if ((b0 & 0xF8u) == 0xF0u) {
    if (len < 4u) {
      goto invalid;
    }
    const uint8_t b1 = s[1];
    const uint8_t b2 = s[2];
    const uint8_t b3 = s[3];
    if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u || (b3 & 0xC0u) != 0x80u) {
      goto invalid;
    }
    if (b0 == 0xF0u && b1 < 0x90u) {
      goto invalid;
    }
    if (b0 == 0xF4u && b1 > 0x8Fu) {
      goto invalid;
    }
    if (b0 > 0xF4u) {
      goto invalid;
    }
    out[0] = b0;
    out[1] = b1;
    out[2] = b2;
    out[3] = b3;
    *out_len = 4u;
    return 4u;
  }

invalid:
  /* Invalid UTF-8 policy: emit U+FFFD and consume 1 byte. */
  out[0] = 0xEFu;
  out[1] = 0xBFu;
  out[2] = 0xBDu;
  *out_len = 3u;
  return 1u;
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
  size_t i = 0u;
  while (i < len) {
    uint8_t glyph[4] = {0u, 0u, 0u, 0u};
    uint8_t glyph_len = 0u;
    const size_t adv = zr_utf8_decode_one(bytes + i, len - i, glyph, &glyph_len);
    if (adv == 0u) {
      break;
    }

    if (cx >= 0 && y >= 0 && cx < (int32_t)fb->cols && y < (int32_t)fb->rows && zr_fb_in_clip(cx, y, clip)) {
      zr_fb_cell_t* c = zr_fb_cell_at(fb, (uint32_t)cx, (uint32_t)y);
      if (c) {
        memset(c->glyph, 0, sizeof(c->glyph));
        memcpy(c->glyph, glyph, (size_t)glyph_len);
        c->glyph_len = glyph_len;
        c->flags = 0u;
        c->style = *style;
      }
    }

    i += adv;
    cx++;
  }
  return ZR_OK;
}
