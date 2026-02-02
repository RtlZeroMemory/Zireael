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

- `include/zr/zr_drawlist.h` (drawlist v1 + v2)
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

Drawlist v1/v2 and event batch v1 are specified by:

- `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md`
- `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`

## Drawlist v2

Drawlist v2 extends v1 with the `SET_CURSOR` opcode (opcode 7). All v1 opcodes
remain unchanged. The engine accepts both versions; version is indicated in the
drawlist header.

```c
typedef struct zr_dl_cmd_set_cursor_t {
  int32_t x;        // 0-based cell; -1 = unchanged
  int32_t y;        // 0-based cell; -1 = unchanged
  uint8_t shape;    // 0=block, 1=underline, 2=bar
  uint8_t visible;  // 0/1
  uint8_t blink;    // 0/1
  uint8_t reserved0; // must be 0
} zr_dl_cmd_set_cursor_t;
```

## Platform capabilities

The engine detects terminal capabilities at init. Wrappers can query these
through `engine_get_metrics()` or by inspecting the returned `plat_caps_t`:

| Capability | Description |
|------------|-------------|
| `color_mode` | 16 / 256 / RGB |
| `supports_mouse` | Mouse input available |
| `supports_bracketed_paste` | Bracketed paste mode |
| `supports_focus_events` | Focus in/out events |
| `supports_osc52` | Clipboard via OSC 52 |
| `supports_sync_update` | Synchronized output (CSI ?2026) |
| `supports_scroll_region` | DECSTBM scroll regions |
| `supports_cursor_shape` | DECSCUSR cursor shapes |

## Damage tracking

The diff renderer tracks changed regions as damage rectangles. Configure via
`zr_limits_t.diff_max_damage_rects`. Metrics report:

- `damage_rects_last_frame` — number of damage rectangles
- `damage_cells_last_frame` — total cells in damage regions
- `damage_full_frame` — 1 if entire frame was dirty

## Performance optimizations

The engine automatically enables optimizations when supported:

- **Synchronized output**: Frames wrapped in sync sequences for tear-free rendering.
- **Scroll regions**: Bulk scrolling via DECSTBM instead of line-by-line redraw.
- **Damage skipping**: Unchanged regions skipped during diff emission.
