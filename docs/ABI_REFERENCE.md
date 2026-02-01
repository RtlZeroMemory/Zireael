# Zireael ABI Reference (Wrapper/FFI Integration)

This document is a language-agnostic reference for integrating with Zireael
from a wrapper (FFI) or a native C application.

The authoritative ABI surface is the public headers; this document summarizes
the binding-critical rules that matter across languages and toolchains.

## Public headers (v1)

Primary entrypoints:

- `src/core/zr_engine.h`
- `src/core/zr_config.h`
- `src/core/zr_metrics.h`

Pinned versions:

- `src/core/zr_version.h`
- `src/unicode/zr_unicode_pins.h`

Binary formats:

- `src/core/zr_drawlist.h` (drawlist v1)
- `src/core/zr_event.h` (packed event batch v1)

## Result / error model

- `ZR_OK == 0` means success.
- Failures are negative `ZR_ERR_*` codes (see `docs/ERROR_CODES_CATALOG.md`).
- `engine_poll_events()` returns:
  - `> 0`: bytes written to `out_buf`
  - `0`: no events available before `timeout_ms`
  - `< 0`: failure (negative `ZR_ERR_*`)

## Version negotiation (engine_create)

`engine_create()` takes `zr_engine_config_t`, which carries requested versions
for the engine ABI and both binary formats.

In v1, requested versions must match the pinned versions exactly; otherwise
`engine_create()` fails with `ZR_ERR_UNSUPPORTED` and performs no partial
effects.

Pinned versions are defined in `src/core/zr_version.h`.

## Ownership and lifetimes

- The engine owns all allocations it makes; callers never free engine memory.
- The engine does not retain pointers into caller buffers beyond a call.
- Callers provide:
  - drawlist bytes to `engine_submit_drawlist()`
  - an event output buffer to `engine_poll_events()`
  - user-event payload bytes to `engine_post_user_event()` (copied during call)

## Threading model

- The engine is single-threaded.
- All `engine_*` calls are engine-thread only, except `engine_post_user_event()`.
- `engine_post_user_event()` is thread-safe and wakes a blocking poll.

## Binary format rules (binding-critical)

- All on-wire/buffer formats are **little-endian**.
- Event records are **4-byte aligned** and self-framed by `zr_ev_record_header_t.size`.
- Reserved/padding fields in v1 structs **MUST be 0** when passed by the caller.

Drawlist v1 and event batch v1 are specified by:

- `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md`
- `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`
