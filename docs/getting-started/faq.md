# FAQ

## Is Zireael a widget framework?

No. Zireael is a **terminal rendering engine** that wrappers/frameworks embed.

## What does "binary in / binary out" mean?

- Wrappers submit a versioned, little-endian **drawlist** command stream.
- Engine returns a versioned, packed **event batch** stream.

This keeps the runtime ABI surface small while allowing high-throughput rendering and input transport.

## Who owns memory?

- Engine owns memory it allocates.
- Caller never frees engine-owned memory.
- Caller provides I/O buffers:
  - drawlist bytes input (`engine_submit_drawlist`)
  - event output buffer (`engine_poll_events`)

## Is the API thread-safe?

Engine calls are single-threaded except:

- `engine_post_user_event()` is intended to be thread-safe and used to wake blocked polling.
- During teardown, `engine_post_user_event()` may return `ZR_ERR_INVALID_ARGUMENT`; stop post threads before `engine_destroy()`.

## How should wrappers handle unknown event types?

Skip them by `zr_ev_record_header_t.size` (4-byte aligned). Unknown types are part of forward-compatibility behavior.

## Why does `engine_poll_events()` sometimes return 0?

`0` means no events were available before `timeout_ms`. This is not an error.

## What does truncation mean in event batches?

If output capacity is insufficient for all events:

- call succeeds
- only complete records are written
- `ZR_EV_BATCH_TRUNCATED` flag is set in batch header

Increase wrapper event buffer if truncation is frequent.

## Where is the authoritative specification?

Internal normative docs in `docs/` are authoritative for implementation behavior:

- start at `docs/00_INDEX.md`
- wrapper-facing ABI pages live under `docs/abi/`

## Next Steps

- [User Guide -> Concepts](../user-guide/concepts.md)
- [ABI -> Drawlist Format](../abi/drawlist-format.md)
- [ABI -> Event Batch Format](../abi/event-batch-format.md)
- [C ABI Reference](../abi/c-abi-reference.md)
