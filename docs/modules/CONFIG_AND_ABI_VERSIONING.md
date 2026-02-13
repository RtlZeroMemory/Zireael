# Module — Config and ABI Versioning

Zireael exposes a stable C ABI surface and versioned binary formats for wrapper consumption.

## Principles

- ABI negotiation happens at engine creation time.
- Binary formats are versioned and self-framed so unknown records can be skipped.
- The engine does not return heap pointers that require the caller to free.

See also:

- `docs/VERSION_PINS.md`
- `docs/HEADER_LAYERING.md`
- `docs/ERROR_CODES_CATALOG.md`
- `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md`
- `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`

## Version negotiation (engine-create)

`engine_create()` takes `zr_engine_config_t`, which includes requested versions for:

- engine ABI (`ZR_ENGINE_ABI_*`)
- drawlist format (`ZR_DRAWLIST_VERSION_V1`)
- packed event batch format (`ZR_EVENT_BATCH_VERSION_V1`)

Negotiation rules:

- Requested engine ABI and event batch versions MUST match pinned versions exactly.
- Drawlist version MUST be one of the supported pinned versions (`ZR_DRAWLIST_VERSION_V1` or `ZR_DRAWLIST_VERSION_V2`).
- If any requested version is not supported, `engine_create()` fails with `ZR_ERR_UNSUPPORTED` and performs no partial effects.

Pinned versions are defined in `src/core/zr_version.h`.

## Config structs (public ABI)

Defined in `src/core/zr_config.h`.

### `zr_engine_config_t`

Passed to `engine_create()` for initial setup and ABI negotiation.

Ownership:

- The engine does not retain pointers into the config; it may copy values it needs.

Limits:

- `cfg->limits` is `zr_limits_t` from `src/util/zr_caps.h` and includes deterministic caps used by the engine.
- `zr_limits_t.out_max_bytes_per_frame` bounds the maximum bytes emitted by `engine_present()` and enables the
  single-flush-per-present contract.
- `zr_limits_validate()` rejects zero-valued caps and enforces only
  `arena_initial_bytes <= arena_max_total_bytes`; drawlist/diff caps are otherwise independent knobs (no cross-field
  dominance constraints).
- `zr_limits_t.dl_max_clip_depth` has a practical execution cap of `64` in `zr_dl_execute()` because the clip stack is
  fixed-size (`kMaxClip + 1` slots). Values `> 64` can still pass `zr_limits_validate()`, but drawlist execution
  deterministically returns `ZR_ERR_LIMIT` before command execution (no framebuffer/cursor partial effects).

ABI requirements:

- `uint8_t` toggles are boolean-like and MUST be encoded as `0` or `1`.
- Reserved/padding fields MUST be `0`.

```c
typedef struct zr_engine_config_t {
  uint32_t requested_engine_abi_major;
  uint32_t requested_engine_abi_minor;
  uint32_t requested_engine_abi_patch;

  uint32_t requested_drawlist_version;
  uint32_t requested_event_batch_version;

  zr_limits_t limits;
  plat_config_t plat;

  uint32_t tab_width;
  uint32_t width_policy;
  uint32_t target_fps;

  uint8_t enable_scroll_optimizations;
  uint8_t enable_debug_overlay;
  uint8_t enable_replay_recording;
  uint8_t wait_for_output_drain;
} zr_engine_config_t;
```

### `zr_engine_runtime_config_t`

Passed to `engine_set_config()` for live reconfiguration.

```c
typedef struct zr_engine_runtime_config_t {
  zr_limits_t limits;
  plat_config_t plat;

  uint32_t tab_width;
  uint32_t width_policy;
  uint32_t target_fps;

  uint8_t enable_scroll_optimizations;
  uint8_t enable_debug_overlay;
  uint8_t enable_replay_recording;
  uint8_t wait_for_output_drain;
} zr_engine_runtime_config_t;
```

Notes:

- `width_policy` is encoded as a fixed-width integer for ABI stability; its values correspond to `zr_width_policy_t`
  enumerators in `src/unicode/zr_width.h` (currently: `ZR_WIDTH_EMOJI_NARROW` or `ZR_WIDTH_EMOJI_WIDE`).
- `enable_replay_recording` is currently a reserved toggle (accepted as 0/1 in v1) and does not change runtime behavior.
- `wait_for_output_drain` enables a bounded wait for output writability before `engine_present()` emits bytes (see
  `docs/modules/DIFF_RENDERER_AND_OUTPUT_EMITTER.md` and `docs/modules/PLATFORM_INTERFACE.md`).

## Runtime capability reporting

The engine exposes runtime terminal capabilities via:

- `engine_get_caps(zr_engine_t* e, zr_terminal_caps_t* out)`

This returns the backend-discovered and engine-derived capability snapshot used for output emission decisions.

## Defaults and validation

Defined in `src/core/zr_config.h`:

- `zr_engine_config_t zr_engine_config_default(void);`
- `zr_result_t zr_engine_config_validate(const zr_engine_config_t* cfg);`
- `zr_result_t zr_engine_runtime_config_validate(const zr_engine_runtime_config_t* cfg);`

v1 validation rules (high level):

- NULL config pointers are invalid (`ZR_ERR_INVALID_ARGUMENT`).
- Reserved/padding fields MUST be zero (`ZR_ERR_INVALID_ARGUMENT`).
- Unsupported requested versions fail with `ZR_ERR_UNSUPPORTED`.
- Cap/limit validation is delegated to `zr_limits_validate()` and is deterministic.
