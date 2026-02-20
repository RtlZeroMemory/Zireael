/*
  src/core/zr_image_kitty.c — Kitty Graphics Protocol byte emitter.

  Why: Encodes deterministic APC sequences for transmit/place/delete actions.
*/

#include "core/zr_image.h"

#include "util/zr_checked.h"

#include <stddef.h>
#include <stdint.h>

enum {
  ZR_KITTY_CHUNK_RAW_MAX = 3072u,
  ZR_KITTY_CHUNK_B64_MAX = ZR_IMAGE_CHUNK_BASE64_MAX,
};

static const uint8_t ZR_ESC = 0x1Bu;

static zr_result_t zr_kitty_write_bytes(zr_sb_t* sb, const void* bytes, size_t len) {
  if (!sb || (!bytes && len != 0u)) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!zr_sb_write_bytes(sb, bytes, len)) {
    return ZR_ERR_LIMIT;
  }
  return ZR_OK;
}

static zr_result_t zr_kitty_write_cstr(zr_sb_t* sb, const char* s) {
  size_t n = 0u;
  if (!sb || !s) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  while (s[n] != '\0') {
    n++;
  }
  return zr_kitty_write_bytes(sb, s, n);
}

static zr_result_t zr_kitty_write_u32(zr_sb_t* sb, uint32_t v) {
  uint8_t tmp[10];
  size_t n = 0u;
  size_t i = 0u;
  if (!sb) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (v == 0u) {
    return zr_kitty_write_bytes(sb, "0", 1u);
  }
  while (v != 0u) {
    tmp[n++] = (uint8_t)('0' + (v % 10u));
    v /= 10u;
  }
  for (i = 0u; i < (n / 2u); i++) {
    uint8_t c = tmp[i];
    tmp[i] = tmp[n - 1u - i];
    tmp[n - 1u - i] = c;
  }
  return zr_kitty_write_bytes(sb, tmp, n);
}

static zr_result_t zr_kitty_write_i32(zr_sb_t* sb, int32_t v) {
  uint32_t mag = 0u;
  if (!sb) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (v < 0) {
    if (!zr_sb_write_u8(sb, (uint8_t)'-')) {
      return ZR_ERR_LIMIT;
    }
    mag = (uint32_t)(0u - (uint32_t)v);
  } else {
    mag = (uint32_t)v;
  }
  return zr_kitty_write_u32(sb, mag);
}

static zr_result_t zr_kitty_begin_apc(zr_sb_t* sb) {
  const uint8_t seq[3] = {ZR_ESC, (uint8_t)'_', (uint8_t)'G'};
  return zr_kitty_write_bytes(sb, seq, sizeof(seq));
}

static zr_result_t zr_kitty_end_apc(zr_sb_t* sb) {
  const uint8_t seq[2] = {ZR_ESC, (uint8_t)'\\'};
  return zr_kitty_write_bytes(sb, seq, sizeof(seq));
}

static zr_result_t zr_kitty_emit_chunk(zr_sb_t* sb, const uint8_t* b64, size_t b64_len, uint32_t m_flag, uint8_t first,
                                       uint32_t kitty_id, uint16_t px_w, uint16_t px_h) {
  zr_result_t rc = ZR_OK;
  if (!sb || !b64) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  rc = zr_kitty_begin_apc(sb);
  if (rc != ZR_OK) {
    return rc;
  }
  if (first != 0u) {
    rc = zr_kitty_write_cstr(sb, "a=t,f=32,s=");
    if (rc != ZR_OK) {
      return rc;
    }
    rc = zr_kitty_write_u32(sb, (uint32_t)px_w);
    if (rc != ZR_OK) {
      return rc;
    }
    rc = zr_kitty_write_cstr(sb, ",v=");
    if (rc != ZR_OK) {
      return rc;
    }
    rc = zr_kitty_write_u32(sb, (uint32_t)px_h);
    if (rc != ZR_OK) {
      return rc;
    }
    rc = zr_kitty_write_cstr(sb, ",i=");
    if (rc != ZR_OK) {
      return rc;
    }
    rc = zr_kitty_write_u32(sb, kitty_id);
    if (rc != ZR_OK) {
      return rc;
    }
    rc = zr_kitty_write_cstr(sb, ",m=");
    if (rc != ZR_OK) {
      return rc;
    }
  } else {
    rc = zr_kitty_write_cstr(sb, "m=");
    if (rc != ZR_OK) {
      return rc;
    }
  }
  rc = zr_kitty_write_u32(sb, m_flag);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_cstr(sb, ";");
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_bytes(sb, b64, b64_len);
  if (rc != ZR_OK) {
    return rc;
  }
  return zr_kitty_end_apc(sb);
}

static zr_result_t zr_kitty_emit_cup(zr_sb_t* sb, uint16_t col, uint16_t row) {
  zr_result_t rc = ZR_OK;
  if (!sb) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  rc = zr_kitty_write_bytes(sb, "\x1b[", 2u);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_u32(sb, (uint32_t)row + 1u);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_bytes(sb, ";", 1u);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_u32(sb, (uint32_t)col + 1u);
  if (rc != ZR_OK) {
    return rc;
  }
  return zr_kitty_write_bytes(sb, "H", 1u);
}

/* Emit kitty transmit APC chunks with <=4096 base64 payload chunks. */
zr_result_t zr_image_kitty_emit_transmit_rgba(zr_sb_t* sb, uint32_t kitty_id, const uint8_t* rgba, uint16_t px_w,
                                              uint16_t px_h, uint16_t dst_cols, uint16_t dst_rows) {
  size_t rgba_len = 0u;
  size_t off = 0u;
  uint8_t first = 1u;

  if (!sb || !rgba || kitty_id == 0u || px_w == 0u || px_h == 0u || dst_cols == 0u || dst_rows == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!zr_checked_mul_size((size_t)px_w, (size_t)px_h, &rgba_len) ||
      !zr_checked_mul_size(rgba_len, (size_t)ZR_IMAGE_RGBA_BYTES_PER_PIXEL, &rgba_len)) {
    return ZR_ERR_LIMIT;
  }

  while (off < rgba_len) {
    size_t take = rgba_len - off;
    uint8_t b64[ZR_KITTY_CHUNK_B64_MAX];
    size_t b64_len = 0u;
    uint32_t m_flag = 0u;
    zr_result_t rc = ZR_OK;

    if (take > ZR_KITTY_CHUNK_RAW_MAX) {
      take = ZR_KITTY_CHUNK_RAW_MAX;
    }
    m_flag = ((off + take) < rgba_len) ? 1u : 0u;

    rc = zr_base64_encode(rgba + off, take, b64, sizeof(b64), &b64_len);
    if (rc != ZR_OK) {
      return rc;
    }
    rc = zr_kitty_emit_chunk(sb, b64, b64_len, m_flag, first, kitty_id, px_w, px_h);
    if (rc != ZR_OK) {
      return rc;
    }
    off += take;
    first = 0u;
  }
  return ZR_OK;
}

/* Place a previously transmitted kitty image at cell coordinates. */
zr_result_t zr_image_kitty_emit_place(zr_sb_t* sb, uint32_t kitty_id, uint16_t dst_col, uint16_t dst_row,
                                      uint16_t dst_cols, uint16_t dst_rows, int8_t z_layer) {
  zr_result_t rc = ZR_OK;
  if (!sb || kitty_id == 0u || dst_cols == 0u || dst_rows == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  rc = zr_kitty_emit_cup(sb, dst_col, dst_row);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_begin_apc(sb);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_cstr(sb, "a=p,i=");
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_u32(sb, kitty_id);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_cstr(sb, ",c=");
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_u32(sb, (uint32_t)dst_cols);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_cstr(sb, ",r=");
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_u32(sb, (uint32_t)dst_rows);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_cstr(sb, ",z=");
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_i32(sb, (int32_t)z_layer);
  if (rc != ZR_OK) {
    return rc;
  }
  return zr_kitty_end_apc(sb);
}

/* Delete a previously transmitted kitty image by image id. */
zr_result_t zr_image_kitty_emit_delete(zr_sb_t* sb, uint32_t kitty_id) {
  zr_result_t rc = ZR_OK;
  if (!sb || kitty_id == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  rc = zr_kitty_begin_apc(sb);
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_cstr(sb, "a=d,d=i,i=");
  if (rc != ZR_OK) {
    return rc;
  }
  rc = zr_kitty_write_u32(sb, kitty_id);
  if (rc != ZR_OK) {
    return rc;
  }
  return zr_kitty_end_apc(sb);
}
