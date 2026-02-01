/*
  src/util/zr_ring.c — Fixed-capacity FIFO ring implementation.

  Why: Keeps FIFO ordering deterministic and avoids state mutation on failed
  push when full.
*/

#include "util/zr_ring.h"

#include "util/zr_checked.h"

#include <string.h>

zr_result_t zr_ring_init(zr_ring_t* r, void* backing_buf, size_t cap_elems, size_t elem_size) {
  if (!r || !backing_buf || elem_size == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  r->data = (unsigned char*)backing_buf;
  r->cap = cap_elems;
  r->len = 0u;
  r->head = 0u;
  r->tail = 0u;
  r->elem_size = elem_size;
  return ZR_OK;
}

void zr_ring_reset(zr_ring_t* r) {
  if (!r) {
    return;
  }
  r->len = 0u;
  r->head = 0u;
  r->tail = 0u;
}

size_t zr_ring_len(const zr_ring_t* r) {
  return r ? r->len : 0u;
}

size_t zr_ring_cap(const zr_ring_t* r) {
  return r ? r->cap : 0u;
}

bool zr_ring_is_empty(const zr_ring_t* r) {
  return !r || r->len == 0u;
}

bool zr_ring_is_full(const zr_ring_t* r) {
  return r && r->cap != 0u && r->len >= r->cap;
}

zr_result_t zr_ring_push(zr_ring_t* r, const void* elem) {
  if (!r || !elem) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (r->cap == 0u || r->len >= r->cap) {
    return ZR_ERR_LIMIT;
  }

  size_t idx = r->tail;
  if (idx >= r->cap) {
    return ZR_ERR_LIMIT;
  }
  size_t off = 0u;
  if (!zr_checked_mul_size(idx, r->elem_size, &off)) {
    return ZR_ERR_LIMIT;
  }

  memcpy(r->data + off, elem, r->elem_size);
  r->tail = (r->tail + 1u) % r->cap;
  r->len++;
  return ZR_OK;
}

bool zr_ring_pop(zr_ring_t* r, void* out_elem) {
  if (!r || !out_elem) {
    return false;
  }
  if (r->len == 0u || r->cap == 0u) {
    return false;
  }

  size_t idx = r->head;
  if (idx >= r->cap) {
    return false;
  }
  size_t off = 0u;
  if (!zr_checked_mul_size(idx, r->elem_size, &off)) {
    return false;
  }

  memcpy(out_elem, r->data + off, r->elem_size);
  r->head = (r->head + 1u) % r->cap;
  r->len--;
  return true;
}

