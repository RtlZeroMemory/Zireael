# C ABI Reference

This page is the wrapper-facing reference for the public C headers under `include/zr/`.

For generated symbol docs, see [API reference (Doxygen)](../api.md).

## Public Headers

Primary wrapper surface:

- `zr_engine.h` - lifecycle, poll/submit/present, metrics, caps, runtime config, debug APIs
- `zr_config.h` - create/runtime config structs and validation
- `zr_result.h` - result/error model
- `zr_version.h` - pinned versions for negotiation

Binary format headers:

- `zr_drawlist.h` - drawlist structs/opcodes
- `zr_event.h` - packed event batch structs/types

Drawlist protocol additions:

- `ZR_DL_OP_DRAW_CANVAS` command payload (`zr_dl_cmd_draw_canvas_t`)
- sub-cell selector enum (`zr_blitter_t`)
- `ZR_DL_OP_DRAW_IMAGE` command payload (`zr_dl_cmd_draw_image_t`)
- `ZR_DL_OP_BLIT_RECT` command payload (`zr_dl_cmd_blit_rect_t`, drawlist v2)
- drawlist v1 image numeric fields in payload:
  - `format`: `0=RGBA`, `1=PNG`
  - `protocol`: `0=auto`, `1=kitty`, `2=sixel`, `3=iterm2`
  - `fit_mode`: `0=fill`, `1=contain`, `2=cover`

Supported drawlist versions: `ZR_DRAWLIST_VERSION_V1`, `ZR_DRAWLIST_VERSION_V2`.

## Result Model

- `ZR_OK` is `0`
- failures are negative `ZR_ERR_*`

Special case:

- `engine_poll_events()` returns:
  - `> 0` bytes written (success)
  - `0` timeout/no queued events (success)
  - `< 0` failure (`ZR_ERR_*`)

## Ownership and Lifetimes

- Engine owns all engine allocations.
- Caller never frees engine memory.
- Engine does not retain drawlist/event output buffer pointers across calls.
- Caller provides buffers for:
  - drawlist input bytes
  - event batch output bytes
  - optional debug export/query payload buffers

## Threading Contract

This section is the wrapper-facing threading source of truth and must stay
aligned with `include/zr/zr_engine.h`.

- Engine is single-threaded by default.
- Call engine APIs from one engine thread.
- `engine_post_user_event()` is intended for cross-thread wake/event injection.
- During teardown, `engine_post_user_event()` may return `ZR_ERR_INVALID_ARGUMENT`.
- Wrappers should quiesce posting threads before `engine_destroy()`.

## Lifecycle

Typical sequence:

1. `zr_engine_config_default()`
2. set requested versions from `zr_version.h`
3. `engine_create()`
4. loop: `engine_poll_events()` -> build drawlist -> `engine_submit_drawlist()` -> `engine_present()`
5. optional telemetry via `engine_get_metrics()` / `engine_get_caps()` / `engine_get_terminal_profile()`
6. `engine_destroy()`

## Core API Contracts

### `engine_create(zr_engine_t** out_engine, const zr_engine_config_t* cfg)`

- Validates config and version negotiation.
- Creates platform backend and enters raw mode.
- Initializes framebuffers, event queue, output/damage buffers, arenas.
- Enqueues an initial `ZR_EV_RESIZE` event.
- On failure: returns error and leaves `*out_engine == NULL`.

Common failures:

- `ZR_ERR_INVALID_ARGUMENT`
- `ZR_ERR_UNSUPPORTED` (version/config mismatch)
- `ZR_ERR_OOM`
- `ZR_ERR_PLATFORM`

### `void engine_destroy(zr_engine_t* e)`

- Safe with `NULL`.
- Leaves raw mode best-effort, destroys platform state, releases all engine-owned memory.
- Should be called after post threads are quiesced.

### `int engine_poll_events(zr_engine_t* e, int timeout_ms, uint8_t* out_buf, int out_cap)`

- Waits for input (bounded by `timeout_ms`) and packs queued events into `out_buf`.
- Uses packed event-batch format (`zr_event.h`).
- Returns bytes written, `0` on timeout/no events, or negative error.

Validation rules:

- `timeout_ms < 0` -> `ZR_ERR_INVALID_ARGUMENT`
- `out_cap < 0` -> `ZR_ERR_INVALID_ARGUMENT`
- `out_cap > 0` with `out_buf == NULL` -> `ZR_ERR_INVALID_ARGUMENT`

Behavior notes:

- If queue already has events, polling does not block.
- Tick events may be injected based on configured frame cadence.
- Truncation is success-mode (`ZR_EV_BATCH_TRUNCATED`) when full batch does not fit.

### `zr_result_t engine_post_user_event(zr_engine_t* e, uint32_t tag, const uint8_t* payload, int payload_len)`

- Appends a wrapper-defined user event (`ZR_EV_USER`) to the queue.
- Best-effort wakes blocked platform wait.
- Payload is copied during call.
- Returns `ZR_ERR_INVALID_ARGUMENT` when teardown has started.

### `zr_result_t engine_submit_drawlist(zr_engine_t* e, const uint8_t* bytes, int bytes_len)`

- Validates drawlist bytes against caps and wire-format rules.
- Executes into staging framebuffer first.
- Commits to next framebuffer only on success (no partial effects).

### `zr_result_t engine_present(zr_engine_t* e)`

- Diffs previous and next framebuffers.
- Emits terminal bytes into internal output buffer.
- Performs a single platform write on success.
- Commits frame/metrics only after successful write.

### `zr_result_t engine_get_metrics(zr_engine_t* e, zr_metrics_t* out_metrics)`

- Prefix-copy semantics: caller sets `out_metrics->struct_size` to receive compatible prefix.
- Engine writes current struct size into copied snapshot field.

### `zr_result_t engine_get_caps(zr_engine_t* e, zr_terminal_caps_t* out_caps)`

- Returns capability snapshot detected from active backend.
- Struct is fixed-width POD (ABI-safe for wrappers).

Fields include output feature gates used by the diff renderer:

- `supports_underline_styles` - enables SGR underline variants (`4:n`)
- `supports_colored_underlines` - enables SGR underline color (`58` / `59`)
- `supports_hyperlinks` - enables OSC 8 hyperlink emission

`engine_get_terminal_profile()` exposes additional image-protocol detection fields used by drawlist v1 image selection:

- `supports_kitty_graphics`
- `supports_sixel`
- `supports_iterm2_images`
- cell pixel metrics (`cell_width_px`, `cell_height_px`) used for protocol scaling targets

### `const zr_terminal_profile_t* engine_get_terminal_profile(const zr_engine_t* e)`

- Returns pointer to engine-owned extended profile snapshot.
- Pointer remains owned by engine and is invalid after `engine_destroy()`.
- Includes terminal identity, probe metadata, and extended render capability fields.

### `zr_result_t engine_set_config(zr_engine_t* e, const zr_engine_runtime_config_t* cfg)`
- Validates runtime config.
- Applies updates with "no partial effects" allocation/commit behavior.
- Platform sub-config changes are rejected with `ZR_ERR_UNSUPPORTED`.

## Debug Trace API

Debug APIs are optional diagnostics hooks:

- `engine_debug_enable` / `engine_debug_disable`
- `engine_debug_query`
- `engine_debug_get_payload`
- `engine_debug_get_stats`
- `engine_debug_export`
- `engine_debug_reset`

Notes:

- Disabled trace behaves as empty trace.
- `engine_debug_export()` returns `0` when tracing is disabled or empty.
- Query can be used in count-only mode by passing `out_headers = NULL`.

## Config Negotiation Essentials

From `zr_engine_config_t` at create time:

- `requested_engine_abi_*` must match pinned ABI macros.
- `requested_drawlist_version` must be `ZR_DRAWLIST_VERSION_V1` or `ZR_DRAWLIST_VERSION_V2`.
  Use `ZR_DRAWLIST_VERSION_V2` when emitting `ZR_DL_OP_BLIT_RECT`.
- `requested_event_batch_version` must match pinned event version.
- `cap_force_flags` / `cap_suppress_flags` apply deterministic capability
  overrides (`suppress` wins over `force`).

Use values directly from `zr_version.h`; do not hardcode copies in wrappers.

## Wrapper Integration Skeleton

```c
zr_engine_config_t cfg = zr_engine_config_default();
cfg.requested_engine_abi_major = ZR_ENGINE_ABI_MAJOR;
cfg.requested_engine_abi_minor = ZR_ENGINE_ABI_MINOR;
cfg.requested_engine_abi_patch = ZR_ENGINE_ABI_PATCH;
cfg.requested_drawlist_version = ZR_DRAWLIST_VERSION_V2;
cfg.requested_event_batch_version = ZR_EVENT_BATCH_VERSION_V1;

zr_engine_t* e = NULL;
zr_result_t rc = engine_create(&e, &cfg);
if (rc != ZR_OK) {
  /* fail early */
}

/* frame loop... */

engine_destroy(e);
```

## Next Steps

- [ABI Policy](abi-policy.md)
- [Versioning](versioning.md)
- [Drawlist Format](drawlist-format.md)
- [Event Batch Format](event-batch-format.md)
