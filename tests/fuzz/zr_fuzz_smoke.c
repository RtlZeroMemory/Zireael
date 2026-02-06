/*
  tests/fuzz/zr_fuzz_smoke.c — Fuzz scaffolding (portable smoke-mode driver).

  Why: Exercises multiple parser/Unicode hot paths in one deterministic,
  portable smoke target so CI catches crashes/progress bugs without libFuzzer.
*/

#include "core/zr_drawlist.h"
#include "core/zr_event_pack.h"
#include "core/zr_input_parser.h"

#include "unicode/zr_grapheme.h"
#include "unicode/zr_utf8.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void zr_fuzz_trap(void) {
#if defined(_MSC_VER)
  __debugbreak();
#elif defined(__GNUC__) || defined(__clang__)
  __builtin_trap();
#else
  volatile int* p = (volatile int*)0;
  *p = 0;
#endif
}

static uint32_t zr_xorshift32(uint32_t* state) {
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

static bool zr_smoke_pack_event(zr_evpack_writer_t* w, const zr_event_t* ev) {
  if (!w || !ev) {
    return false;
  }

  switch (ev->type) {
  case ZR_EV_KEY:
    return zr_evpack_append_record(w, ZR_EV_KEY, ev->time_ms, ev->flags, &ev->u.key, sizeof(ev->u.key));
  case ZR_EV_TEXT:
    return zr_evpack_append_record(w, ZR_EV_TEXT, ev->time_ms, ev->flags, &ev->u.text, sizeof(ev->u.text));
  case ZR_EV_MOUSE:
    return zr_evpack_append_record(w, ZR_EV_MOUSE, ev->time_ms, ev->flags, &ev->u.mouse, sizeof(ev->u.mouse));
  case ZR_EV_RESIZE:
    return zr_evpack_append_record(w, ZR_EV_RESIZE, ev->time_ms, ev->flags, &ev->u.resize, sizeof(ev->u.resize));
  case ZR_EV_TICK:
    return zr_evpack_append_record(w, ZR_EV_TICK, ev->time_ms, ev->flags, &ev->u.tick, sizeof(ev->u.tick));
  default:
    return true;
  }
}

static void zr_smoke_check_utf8_progress(const uint8_t* data, size_t size) {
  size_t off = 0u;
  while (off < size) {
    const zr_utf8_decode_result_t r = zr_utf8_decode_one(data + off, size - off);
    if (r.size == 0u || (size_t)r.size > (size - off)) {
      zr_fuzz_trap();
    }
    off += (size_t)r.size;
  }
}

static void zr_smoke_check_grapheme_progress(const uint8_t* data, size_t size) {
  zr_grapheme_iter_t it;
  zr_grapheme_t g;
  zr_grapheme_iter_init(&it, data, size);

  size_t total = 0u;
  size_t count = 0u;
  while (zr_grapheme_next(&it, &g)) {
    if (g.size == 0u || g.offset != total) {
      zr_fuzz_trap();
    }
    total += g.size;
    count++;
    if (count > size + 1u) {
      zr_fuzz_trap();
    }
  }
  if (total != size) {
    zr_fuzz_trap();
  }
}

static void zr_smoke_check_drawlist_determinism(const uint8_t* data, size_t size) {
  zr_limits_t lim = zr_limits_default();
  lim.dl_max_total_bytes = (size > (size_t)UINT32_MAX) ? UINT32_MAX : (uint32_t)size;
  lim.dl_max_cmds = 64u;
  lim.dl_max_strings = 64u;
  lim.dl_max_blobs = 64u;
  lim.dl_max_clip_depth = 16u;
  lim.dl_max_text_run_segments = 64u;

  zr_dl_view_t v1;
  zr_dl_view_t v2;
  const zr_result_t r1 = zr_dl_validate(data, size, &lim, &v1);
  const zr_result_t r2 = zr_dl_validate(data, size, &lim, &v2);
  if (r1 != r2) {
    zr_fuzz_trap();
  }
  if (r1 == ZR_OK) {
    if (memcmp(&v1.hdr, &v2.hdr, sizeof(v1.hdr)) != 0 || v1.cmd_bytes_len != v2.cmd_bytes_len ||
        v1.strings_count != v2.strings_count || v1.blobs_count != v2.blobs_count) {
      zr_fuzz_trap();
    }
  }
}

static void zr_smoke_check_input_parser(const uint8_t* data, size_t size) {
  enum {
    ZR_SMOKE_EVENT_CAP = 64,
    ZR_SMOKE_USER_CAP = 256,
    /* Worst-case packed event in this harness is mouse (record header + payload). */
    ZR_SMOKE_MAX_RECORD_PADDED_BYTES = ((uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_mouse_t)) + 3u) & ~3u,
    ZR_SMOKE_OUT_CAP = (uint32_t)sizeof(zr_evbatch_header_t) + (ZR_SMOKE_EVENT_CAP * ZR_SMOKE_MAX_RECORD_PADDED_BYTES),
  };

  zr_event_t ev_store1[ZR_SMOKE_EVENT_CAP];
  zr_event_t ev_store2[ZR_SMOKE_EVENT_CAP];
  uint8_t user1[ZR_SMOKE_USER_CAP];
  uint8_t user2[ZR_SMOKE_USER_CAP];
  zr_event_queue_t q1;
  zr_event_queue_t q2;
  if (zr_event_queue_init(&q1, ev_store1, ZR_SMOKE_EVENT_CAP, user1, ZR_SMOKE_USER_CAP) != ZR_OK ||
      zr_event_queue_init(&q2, ev_store2, ZR_SMOKE_EVENT_CAP, user2, ZR_SMOKE_USER_CAP) != ZR_OK) {
    zr_fuzz_trap();
  }

  zr_input_parse_bytes(&q1, data, size, 0u);
  zr_input_parse_bytes(&q2, data, size, 0u);

  zr_event_t ev_store3[ZR_SMOKE_EVENT_CAP];
  uint8_t user3[ZR_SMOKE_USER_CAP];
  zr_event_queue_t q3;
  if (zr_event_queue_init(&q3, ev_store3, ZR_SMOKE_EVENT_CAP, user3, ZR_SMOKE_USER_CAP) != ZR_OK) {
    zr_fuzz_trap();
  }
  const size_t consumed = zr_input_parse_bytes_prefix(&q3, data, size, 0u);
  if (consumed > size) {
    zr_fuzz_trap();
  }
  zr_input_parse_bytes(&q3, data + consumed, size - consumed, 0u);

  uint8_t out1[ZR_SMOKE_OUT_CAP];
  uint8_t out2[ZR_SMOKE_OUT_CAP];
  memset(out1, 0xA5, sizeof(out1));
  memset(out2, 0xA5, sizeof(out2));

  zr_evpack_writer_t w1;
  zr_evpack_writer_t w2;
  if (zr_evpack_begin(&w1, out1, sizeof(out1)) != ZR_OK || zr_evpack_begin(&w2, out2, sizeof(out2)) != ZR_OK) {
    zr_fuzz_trap();
  }

  zr_event_t ev;
  while (zr_event_queue_peek(&q1, &ev)) {
    if (!zr_smoke_pack_event(&w1, &ev)) {
      zr_fuzz_trap();
    }
    (void)zr_event_queue_pop(&q1, &ev);
  }
  while (zr_event_queue_peek(&q2, &ev)) {
    if (!zr_smoke_pack_event(&w2, &ev)) {
      zr_fuzz_trap();
    }
    (void)zr_event_queue_pop(&q2, &ev);
  }

  const size_t n1 = zr_evpack_finish(&w1);
  const size_t n2 = zr_evpack_finish(&w2);
  if (n1 != n2 || memcmp(out1, out2, n1) != 0) {
    zr_fuzz_trap();
  }
}

static void zr_fuzz_target_one_input(const uint8_t* data, size_t size) {
  zr_smoke_check_utf8_progress(data, size);
  zr_smoke_check_grapheme_progress(data, size);
  zr_smoke_check_drawlist_determinism(data, size);
  zr_smoke_check_input_parser(data, size);
}

int main(void) {
  /* Deterministic run: fixed iteration count and PRNG seed. */
  enum { kIters = 1000, kMaxSize = 512 };
  uint32_t seed = 0xC0FFEEu;
  uint8_t buf[kMaxSize];

  for (int i = 0; i < kIters; i++) {
    const size_t sz = (size_t)(zr_xorshift32(&seed) % (uint32_t)kMaxSize);
    for (size_t j = 0; j < sz; j++) {
      buf[j] = (uint8_t)(zr_xorshift32(&seed) & 0xFFu);
    }
    (void)zr_fuzz_target_one_input(buf, sz);
  }

  return 0;
}
