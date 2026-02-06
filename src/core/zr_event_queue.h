/*
  src/core/zr_event_queue.h — Normalized event queue (bounded, deterministic).

  Why: Provides cap-bounded storage with deterministic coalescing/backpressure
  and a thread-safe user-event injection path (payload is copied on enqueue).
*/

#ifndef ZR_CORE_ZR_EVENT_QUEUE_H_INCLUDED
#define ZR_CORE_ZR_EVENT_QUEUE_H_INCLUDED

#include "core/zr_event.h"
#include "util/zr_result.h"

#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>

typedef struct zr_event_t {
  zr_event_type_t type;
  uint32_t time_ms;
  uint32_t flags;

  union {
    zr_ev_key_t key;
    zr_ev_text_t text;
    struct {
      zr_ev_paste_t hdr; /* includes byte_len */
      uint32_t payload_off;
      uint32_t reserved0;
    } paste;
    zr_ev_mouse_t mouse;
    zr_ev_resize_t resize;
    zr_ev_tick_t tick;
    struct {
      zr_ev_user_t hdr; /* includes tag + byte_len */
      uint32_t payload_off;
      uint32_t reserved0;
    } user;
  } u;
} zr_event_t;

typedef struct zr_event_queue_t {
  atomic_flag lock;

  zr_event_t* events;
  uint32_t cap;
  uint32_t head;
  uint32_t count;

  uint8_t* user_bytes;
  uint32_t user_bytes_cap;
  uint32_t user_head;
  uint32_t user_tail;
  uint32_t user_used;
  uint32_t user_pad_end; /* bytes reserved at end after wrap (variable-size ring needs explicit pad tracking) */

  uint32_t dropped_total;
  uint32_t dropped_due_to_full;
  uint32_t dropped_user_due_to_full;
  uint32_t dropped_coalesce_candidates;
} zr_event_queue_t;

/*
  zr_event_queue_init:
    - Caller supplies all storage (no heap allocation).
    - user_bytes is used for variable-length payload copies (USER/PASTE).
*/
zr_result_t zr_event_queue_init(zr_event_queue_t* q, zr_event_t* events, uint32_t events_cap, uint8_t* user_bytes,
                                uint32_t user_bytes_cap);

/* Engine-thread enqueue with deterministic coalescing/drop policy. */
zr_result_t zr_event_queue_push(zr_event_queue_t* q, const zr_event_t* ev);

/*
  Engine-thread enqueue (no-drop):
    - Deterministic coalescing still applies.
    - If the queue is full, returns ZR_ERR_LIMIT and does NOT drop any existing events.
*/
zr_result_t zr_event_queue_try_push_no_drop(zr_event_queue_t* q, const zr_event_t* ev);

/*
  Thread-safe user event injection:
    - copies payload bytes into the queue's user_bytes ring
    - returns ZR_ERR_LIMIT if queue or user_bytes capacity is exceeded
    - does not drop existing events to make room
*/
zr_result_t zr_event_queue_post_user(zr_event_queue_t* q, uint32_t time_ms, uint32_t tag, const uint8_t* payload,
                                     uint32_t payload_len);

/*
  Engine-thread bracketed paste enqueue:
    - copies paste bytes into the queue's user_bytes ring
    - returns ZR_ERR_LIMIT if user_bytes capacity is exceeded
    - may drop the oldest event if the event queue is full
*/
zr_result_t zr_event_queue_post_paste(zr_event_queue_t* q, uint32_t time_ms, const uint8_t* bytes, uint32_t byte_len);

/* Pop/peek in FIFO order. */
bool zr_event_queue_peek(const zr_event_queue_t* q, zr_event_t* out_ev);
bool zr_event_queue_pop(zr_event_queue_t* q, zr_event_t* out_ev);

/*
  Return a thread-safe snapshot of queued event count.

  Why: engine_poll_events() may run concurrently with engine_post_user_event().
  Reading q->count without synchronization is a data race under C11.
*/
uint32_t zr_event_queue_count(const zr_event_queue_t* q);

/*
  Returns a borrowed pointer to the user payload bytes for a ZR_EV_USER event.
  The pointer remains valid until the corresponding event is popped/dropped.
*/
bool zr_event_queue_user_payload_view(const zr_event_queue_t* q, const zr_event_t* ev, const uint8_t** out_bytes,
                                      uint32_t* out_len);

/*
  Returns a borrowed pointer to the paste payload bytes for a ZR_EV_PASTE event.
  The pointer remains valid until the corresponding event is popped/dropped.
*/
bool zr_event_queue_paste_payload_view(const zr_event_queue_t* q, const zr_event_t* ev, const uint8_t** out_bytes,
                                       uint32_t* out_len);

#endif /* ZR_CORE_ZR_EVENT_QUEUE_H_INCLUDED */
