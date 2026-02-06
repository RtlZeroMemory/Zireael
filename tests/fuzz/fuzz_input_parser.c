/*
  tests/fuzz/fuzz_input_parser.c — Input parser fuzz target (smoke-mode).

  Why: Validates that the terminal input parser (VT sequences) never crashes,
  hangs, or produces non-deterministic output when fed arbitrary bytes.

  Invariants verified:
    - Parser never crashes on malformed input
    - Same input always produces same event queue (determinism)
    - Serialized event batches are byte-identical for same input
*/

#include "core/zr_event_pack.h"
#include "core/zr_input_parser.h"

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

/* Pack an event into the event batch writer for determinism comparison. */
static bool zr_pack_event(zr_evpack_writer_t* w, const zr_event_queue_t* q, const zr_event_t* ev) {
  (void)q;
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
  case ZR_EV_USER: {
    const uint8_t* payload = NULL;
    uint32_t payload_len = 0u;
    if (!zr_event_queue_user_payload_view(q, ev, &payload, &payload_len)) {
      return false;
    }
    return zr_evpack_append_record2(w, ZR_EV_USER, ev->time_ms, ev->flags, &ev->u.user.hdr, sizeof(ev->u.user.hdr),
                                    payload, (size_t)payload_len);
  }
  default:
    /* Unknown types are deterministically ignored by the smoke target. */
    return true;
  }
}

/*
 * Fuzz one input: parse bytes into events twice, serialize, and compare.
 *
 * Checks:
 *   1. Parser doesn't crash on arbitrary bytes
 *   2. Same input produces same events (determinism)
 *   3. Serialized event batches are byte-identical
 */
static void zr_fuzz_one(const uint8_t* data, size_t size) {
  enum { kEventCap = 64, kUserBytesCap = 256, kOutCap = 1024 };

  /* Two independent event queues for determinism comparison. */
  zr_event_t ev_storage1[kEventCap];
  zr_event_t ev_storage2[kEventCap];
  uint8_t user_bytes1[kUserBytesCap];
  uint8_t user_bytes2[kUserBytesCap];

  zr_event_queue_t q1;
  zr_event_queue_t q2;
  if (zr_event_queue_init(&q1, ev_storage1, kEventCap, user_bytes1, kUserBytesCap) != ZR_OK ||
      zr_event_queue_init(&q2, ev_storage2, kEventCap, user_bytes2, kUserBytesCap) != ZR_OK) {
    zr_fuzz_trap();
  }

  zr_input_parse_bytes(&q1, data, size, 0u);
  zr_input_parse_bytes(&q2, data, size, 0u);

  uint8_t out1[kOutCap];
  uint8_t out2[kOutCap];
  memset(out1, 0xA5, sizeof(out1));
  memset(out2, 0xA5, sizeof(out2));

  zr_evpack_writer_t w1;
  zr_evpack_writer_t w2;
  if (zr_evpack_begin(&w1, out1, sizeof(out1)) != ZR_OK || zr_evpack_begin(&w2, out2, sizeof(out2)) != ZR_OK) {
    zr_fuzz_trap();
  }

  zr_event_t ev;
  while (zr_event_queue_peek(&q1, &ev)) {
    (void)zr_pack_event(&w1, &q1, &ev);
    (void)zr_event_queue_pop(&q1, &ev);
  }
  while (zr_event_queue_peek(&q2, &ev)) {
    (void)zr_pack_event(&w2, &q2, &ev);
    (void)zr_event_queue_pop(&q2, &ev);
  }

  const size_t n1 = zr_evpack_finish(&w1);
  const size_t n2 = zr_evpack_finish(&w2);

  if (n1 != n2) {
    zr_fuzz_trap();
  }
  if (memcmp(out1, out2, n1) != 0) {
    zr_fuzz_trap();
  }
}

int main(void) {
  enum { kIters = 1000, kMaxSize = 512 };
  uint32_t seed = 0x1A2B3C4Du;
  uint8_t buf[kMaxSize];

  for (int i = 0; i < kIters; i++) {
    const size_t sz = (size_t)(zr_xorshift32(&seed) % (uint32_t)kMaxSize);
    for (size_t j = 0; j < sz; j++) {
      buf[j] = (uint8_t)(zr_xorshift32(&seed) & 0xFFu);
    }
    zr_fuzz_one(buf, sz);
  }
  return 0;
}
