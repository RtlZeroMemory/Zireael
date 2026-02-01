# Zireael — C Core Terminal Engine

Zireael is a cross-platform **terminal UI core engine** for Windows / Linux / macOS.

It is designed to be embedded: you drive it by submitting a **binary drawlist** and you read back a **packed event batch**. The engine owns terminal I/O (raw mode, output emission, input bytes) and provides a deterministic, cap-bounded core suitable for wrappers in other languages.

## Overview

At a glance:

- **Engine-only C library**: no TypeScript, no Node tooling, no GUI toolkit.
- **Stable ABI**: callers interact via a small `engine_*` API surface.
- **Explicit ownership**: caller provides input/output buffers; the engine never returns heap pointers that require caller `free()`.
- **Deterministic core**: fixed inputs + fixed caps/config ⇒ fixed outputs.
- **Hard platform boundary**: OS headers live only under `src/platform/**`.

## Architecture

### Component map

```
+------------------+         +---------------------------------------------+
| Caller / Wrapper |         |                   Zireael                    |
|  (any language)  |         |---------------------------------------------|
|                  | drawlist|  core: drawlist → framebuffer → diff → emit  |
| submit bytes     |-------> |  unicode: UTF-8 / graphemes / width / wrap   |
|                  |         |  util: arenas / containers / checked math     |
| poll events      | <------ |  platform: POSIX / Win32 backend (raw I/O)    |
+------------------+         +---------------------------------------------+
```

### Data flow

Output path (rendering):

```
drawlist bytes
   │
   v
validate (bounds/caps/version)
   │
   v
execute → next framebuffer
   │
   v
diff(prev, next) → VT/ANSI byte stream → platform_write() (single flush)
   │
   v
swap(prev, next)
```

Input path (events):

```
platform_read() → input bytes → parse/normalize → queue/coalesce
                                         │
                                         v
                               pack to event-batch ABI
                                         │
                                         v
                              caller-provided output buffer
```

### Repository layout

```
src/
  util/              arenas, bounded containers, checked helpers, logging
  unicode/           UTF-8 decode, graphemes, width policy, wrapping
  core/              engine ABI, events, drawlist, framebuffer, diff renderer
  platform/
    posix/           termios raw mode, poll/select, wake (self-pipe/eventfd)
    win32/           VT mode enable, wait/wake, console mode save/restore
tests/
  unit/ golden/ fuzz/ integration/
examples/
```

## Public API (C ABI)

Headers:

- `src/core/zr_engine.h` (primary entrypoints)
- `src/core/zr_config.h` (configuration)
- `src/core/zr_event.h` (packed event ABI types)
- `src/core/zr_drawlist.h` (drawlist ABI types)
- `src/platform/zr_platform.h` (core-facing platform boundary; OS-header-free)

Core entrypoints (summary):

```c
typedef struct zr_engine_t zr_engine_t;
typedef int zr_result_t;

zr_result_t engine_create(zr_engine_t** out_engine, const zr_engine_config_t* cfg);
void        engine_destroy(zr_engine_t* e);

int         engine_poll_events(zr_engine_t* e, int timeout_ms, uint8_t* out_buf, int out_cap);
zr_result_t engine_post_user_event(zr_engine_t* e, uint32_t tag, const uint8_t* payload, int payload_len);

zr_result_t engine_submit_drawlist(zr_engine_t* e, const uint8_t* bytes, int bytes_len);
zr_result_t engine_present(zr_engine_t* e);

zr_result_t engine_get_metrics(zr_engine_t* e, zr_metrics_t* out_metrics);
zr_result_t engine_set_config(zr_engine_t* e, const zr_engine_runtime_config_t* cfg);
```

Return conventions:

- `ZR_OK == 0` means success.
- Negative values are failures (`ZR_ERR_*`).
- `engine_poll_events` returns:
  - `> 0`: bytes written to `out_buf`
  - `0`: no events before `timeout_ms`
  - `< 0`: failure (negative `ZR_ERR_*`)

## Ownership, caps, and “no partial effects”

- The engine owns all allocations it makes; callers never free engine memory.
- The engine does not retain pointers into caller buffers beyond the call.
- Callers provide:
  - drawlist bytes (`engine_submit_drawlist`)
  - packed event output buffer (`engine_poll_events`)
  - user-event payload bytes (`engine_post_user_event`, copied during the call)
- Failures are **deterministic** and, by default, have **no partial effects** (the drawlist path validates fully before mutating the `next` framebuffer).
- Resource usage is cap-bounded via `zr_limits_t` (e.g., max bytes per frame, max events per poll, arena growth caps).

## Binary formats

Zireael uses versioned, little-endian binary formats designed for:

- bounds-checked parsing (no UB, no unaligned loads via casts)
- deterministic rejection of malformed inputs
- forward/backward compatibility policies that are explicit per format

### Drawlist (wrapper → engine)

The drawlist is a self-framed command stream with a fixed header:

```
┌───────────────┐
│ header (v1)    │ magic/version/offsets/sizes
├───────────────┤
│ cmd stream     │ [ {opcode,size,flags} payload ]...
├───────────────┤
│ string table   │ spans + concatenated UTF-8 bytes
├───────────────┤
│ blob table     │ spans + concatenated binary blobs
└───────────────┘
```

Unknown opcode policy (v1): reject with `ZR_ERR_UNSUPPORTED`.

### Packed event batch (engine → wrapper)

Events are written as a batch header followed by 4-byte-aligned records:

```
┌───────────────┐
│ batch header   │ magic/version/total_size/event_count/flags
├───────────────┤
│ record 0       │ {type,size,time_ms,flags} payload...
├───────────────┤
│ record 1       │ ...
└───────────────┘
```

Truncation policy (v1): if the caller buffer cannot fit all events, the engine writes as many **complete** records as fit, sets the batch `TRUNCATED` flag, and returns the bytes written.

## Threading model

- The engine is **single-threaded**: all `engine_*` calls are engine-thread only, except:
  - `engine_post_user_event`, which is thread-safe and wakes a blocking poll.
- The engine does not invoke user callbacks from non-engine threads (including logging).

## Unicode and determinism

Text handling is deterministic and pinned:

- Unicode data version: **15.1.0**
- Default emoji width policy: **emoji wide**
- Invalid UTF-8 policy: emit `U+FFFD`, mark invalid, and consume 1 byte

These pins exist to keep wrapping/measurement/rendering stable across platforms and toolchains.

## Build

Zireael uses CMake (C11). Presets are defined in `CMakePresets.json` (Ninja generator).

Windows note (important):

- The `windows-clangcl-*` presets use `Ninja` + `clang-cl`, which requires an MSVC/Windows-SDK environment for linking.
- In a regular PowerShell, run `.\scripts\vsdev.ps1` once before configuring/building.

Configure:

```text
.\scripts\vsdev.ps1
cmake --preset windows-clangcl-debug
cmake --preset posix-clang-debug
```

Build:

```text
cmake --build --preset windows-clangcl-debug
cmake --build --preset posix-clang-debug
```

Guardrails (platform boundary + libc policy):

```text
bash scripts/guardrails.sh
```

Code standards (readability + safety):

- `docs/CODE_STANDARDS.md`

CMake options:

- `ZIREAEL_BUILD_SHARED` (default: OFF)
- `ZIREAEL_BUILD_EXAMPLES` (default: ON)
- `ZIREAEL_BUILD_TESTS` (default: ON)
- `ZIREAEL_WARNINGS_AS_ERRORS` (default: OFF; CI)
- `ZIREAEL_SANITIZE_ADDRESS` (default: OFF; Clang/GCC only)
- `ZIREAEL_SANITIZE_UNDEFINED` (default: OFF; Clang/GCC only)

Embedding as a subproject:

```cmake
add_subdirectory(path/to/zireael)
target_link_libraries(my_app PRIVATE Zireael::zireael)
```

## Usage (typical loop)

```c
zr_engine_config_t cfg = zr_engine_config_default();
cfg.requested_engine_abi_major = ZR_ENGINE_ABI_MAJOR;
cfg.requested_drawlist_version = ZR_DRAWLIST_VERSION_V1;
cfg.requested_event_batch_version = ZR_EVENT_BATCH_VERSION_V1;

zr_engine_t* e = NULL;
zr_result_t rc = engine_create(&e, &cfg);
if (rc != ZR_OK) { /* handle error */ return 1; }

for (;;) {
  uint8_t evbuf[64 * 1024];
  int evlen = engine_poll_events(e, /*timeout_ms=*/16, evbuf, (int)sizeof evbuf);
  if (evlen < 0) { /* handle error */ break; }

  // application logic consumes packed events in evbuf[0..evlen)
  // application produces a drawlist byte buffer for the next frame

  rc = engine_submit_drawlist(e, drawlist_bytes, drawlist_len);
  if (rc != ZR_OK) { /* handle error */ break; }

  rc = engine_present(e);
  if (rc != ZR_OK) { /* handle error */ break; }
}

engine_destroy(e);
```

## Testing

Zireael is tested via:

- **Unit tests**: pure logic (util/unicode/core), deterministic, no OS headers.
- **Golden tests**: diff output compared byte-for-byte.
- **Fuzz tests**: drawlist parser, UTF-8 decoder, input parser (cap-respecting; no crash/hang).
- **Integration tests**: PTY/ConPTY headless tests for raw mode lifecycle and wake behavior.

Run tests (after configuring/building a preset):

```text
ctest --test-dir out/build/<preset> --output-on-failure
```

## Contributing guidelines (high-level)

- Keep OS headers out of `src/core`, `src/unicode`, `src/util`.
- Treat drawlist/event bytes as untrusted input: validate bounds and overflow before use.
- Prefer fixed-size, caller-provided buffers on the ABI boundary.
- Avoid per-frame heap churn; use arenas and bounded containers.

## License

Apache-2.0 (see `LICENSE`).
