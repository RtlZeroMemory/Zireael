# Configuration

Engine configuration is passed to `engine_create()` and controls version negotiation, resource limits, and behavior.

## zr_engine_config_t (52 bytes)

```
Offset  Size  Field                         Description
──────  ────  ─────                         ───────────
0x00    4     requested_engine_abi_major    Must match ZR_ENGINE_ABI_MAJOR (1)
0x04    4     requested_engine_abi_minor    Must be <= ZR_ENGINE_ABI_MINOR
0x08    4     requested_engine_abi_patch    Informational

0x0C    4     requested_drawlist_version    e.g. ZR_DRAWLIST_VERSION_V1 (1)
0x10    4     requested_event_batch_version e.g. ZR_EVENT_BATCH_VERSION_V1 (1)

0x14    36    limits                        zr_limits_t (resource caps)
0x38    20    plat                          plat_config_t (platform config)

0x4C    4     tab_width                     Tab stop width (default: 8)
0x50    4     width_policy                  Unicode width calculation policy

0x54    4     target_fps                    Target frame rate (default: 60)

0x58    1     enable_scroll_optimizations   0 or 1
0x59    1     enable_debug_overlay          0 or 1
0x5A    1     enable_replay_recording       0 or 1
0x5B    1     _pad0                         Must be 0
```

## zr_limits_t (36 bytes)

Resource caps for deterministic behavior:

```
Offset  Size  Field                    Description
──────  ────  ─────                    ───────────
0x00    4     arena_max_total_bytes    Max arena memory
0x04    4     arena_initial_bytes      Initial arena size

0x08    4     out_max_bytes_per_frame  Max output per present()

0x0C    4     dl_max_total_bytes       Max drawlist size
0x10    4     dl_max_cmds              Max commands per drawlist
0x14    4     dl_max_strings           Max string table entries
0x18    4     dl_max_blobs             Max blob table entries
0x1C    4     dl_max_clip_depth        Max nested clips
0x20    4     dl_max_text_run_segments Max text run segments
```

Use `zr_limits_default()` for sensible defaults.

## plat_config_t (20 bytes)

Platform capability requests:

```
Offset  Size  Field             Description
──────  ────  ─────             ───────────
0x00    4     color_mode        Requested color mode (16, 256, RGB)
0x04    4     mouse             Enable mouse input (0/1)
0x08    4     bracketed_paste   Enable bracketed paste (0/1)
0x0C    4     focus_events      Enable focus tracking (0/1)
0x10    4     osc52             Enable OSC52 clipboard (0/1)
```

## Version Negotiation

At `engine_create()`:

1. Engine checks `requested_engine_abi_major == ZR_ENGINE_ABI_MAJOR`
2. Engine checks `requested_engine_abi_minor <= ZR_ENGINE_ABI_MINOR`
3. Engine checks `requested_drawlist_version` is supported
4. Engine checks `requested_event_batch_version` is supported

On mismatch, returns `ZR_ERR_UNSUPPORTED`.

## Current Versions

| Constant | Value |
|----------|-------|
| `ZR_ENGINE_ABI_MAJOR` | 1 |
| `ZR_ENGINE_ABI_MINOR` | 0 |
| `ZR_ENGINE_ABI_PATCH` | 0 |
| `ZR_DRAWLIST_VERSION_V1` | 1 |
| `ZR_EVENT_BATCH_VERSION_V1` | 1 |

## Example

```c
zr_engine_config_t cfg = zr_engine_config_default();
cfg.requested_engine_abi_major = 1;
cfg.requested_engine_abi_minor = 0;
cfg.requested_drawlist_version = 1;
cfg.requested_event_batch_version = 1;

cfg.limits.dl_max_total_bytes = 64 * 1024;  // 64KB drawlists
cfg.plat.color_mode = 2;  // 256 colors
cfg.plat.mouse = 1;

zr_engine_t* engine = NULL;
zr_result_t r = engine_create(&engine, &cfg);
if (r != ZR_OK) {
    // Handle error
}
```

## Runtime Reconfiguration

`engine_set_config()` accepts `zr_engine_runtime_config_t`, which is a subset of the create-time config (excludes version negotiation fields).
