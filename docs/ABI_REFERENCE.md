# Zireael ABI Reference (Wrapper/FFI Integration)

This document is a language-agnostic reference for integrating with Zireael
from a wrapper (FFI) or a native C application.

The authoritative ABI surface is the public headers; this document summarizes
the binding-critical rules that matter across languages and toolchains.

## Public headers (v1)

Primary entrypoints:

- `include/zr/zr_engine.h`
- `include/zr/zr_config.h`
- `include/zr/zr_metrics.h`
- `include/zr/zr_result.h`

Pinned versions:

- `include/zr/zr_version.h`
- `src/unicode/zr_unicode_pins.h` (internal pin; exposed indirectly via core behavior)

Binary formats:

- `include/zr/zr_drawlist.h` (drawlist v1)
- `include/zr/zr_event.h` (packed event batch v1)

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

Pinned versions are defined in `include/zr/zr_version.h` (internal includes may
provide compatibility paths, but the public root is `include/zr/`).

## Ownership and lifetimes

- The engine owns all allocations it makes; callers never free engine memory.
- The engine does not retain pointers into caller buffers beyond a call.
- Callers provide:
  - drawlist bytes to `engine_submit_drawlist()`
  - an event output buffer to `engine_poll_events()`
  - user-event payload bytes to `engine_post_user_event()` (copied during call)

## Threading model

Canonical source:

- `include/zr/zr_engine.h` (public header contract)
- `docs/abi/c-abi-reference.md` (wrapper-facing contract)

- The engine is single-threaded.
- All `engine_*` calls are engine-thread only, except `engine_post_user_event()`.
- `engine_post_user_event()` is thread-safe and wakes a blocking poll.
- During teardown, `engine_post_user_event()` may return `ZR_ERR_INVALID_ARGUMENT`.
- Wrappers should stop post threads before calling `engine_destroy()`.

## Binary format rules (binding-critical)

- All on-wire/buffer formats are **little-endian**.
- Event records are **4-byte aligned** and self-framed by `zr_ev_record_header_t.size`.
- Reserved/padding fields in v1 structs **MUST be 0** when passed by the caller.
- `ZR_EV_TEXT.codepoint` carries Unicode scalar values; invalid UTF-8 input emits U+FFFD.

Drawlist v1 and event batch v1 are specified by:

- `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md`
- `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`

## Drawlist v1/v2

Drawlist v1 includes the `SET_CURSOR` opcode (opcode 7). All v1 opcodes remain
unchanged. Drawlist v2 adds `ZR_DL_OP_BLIT_RECT` (opcode 14). The engine
accepts drawlists whose header version is `ZR_DRAWLIST_VERSION_V1` or
`ZR_DRAWLIST_VERSION_V2`.

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

## Drawlist v1 style extension

Drawlist v1 style payloads include:

- underline color (`underline_rgb`)
- hyperlink URI reference (`link_uri_ref`)
- optional hyperlink id reference (`link_id_ref`)

Hyperlink references use the drawlist string table and are resolved into
framebuffer-owned link refs during execute.

## Platform capabilities

The engine detects terminal capabilities at init. Wrappers can query these
through `engine_get_caps()` (which returns `zr_terminal_caps_t`):

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
| `supports_underline_styles` | SGR underline variants (`4:n`) |
| `supports_colored_underlines` | SGR underline color (`58`/`59`) |
| `supports_hyperlinks` | OSC 8 hyperlink open/close emission |

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
