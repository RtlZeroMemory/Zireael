# Configuration

Configuration is passed to `engine_create()` and controls version negotiation, resource limits, platform behavior, and pinned text policy.

The tables below describe the **in-memory C struct layout** used by the public ABI (as defined in `include/zr/*.h`).

## zr_engine_config_t (80 bytes)

`zr_engine_config_t` is validated by `engine_create()`:

- version pins must match the engine's pinned ABI/format versions
- limits must be non-zero and internally consistent
- platform flags are 0/1 bytes, and padding must be zero

```
Offset  Size  Field                           Description
──────  ────  ─────                           ───────────
0x00    4     requested_engine_abi_major      Must equal ZR_ENGINE_ABI_MAJOR (1)
0x04    4     requested_engine_abi_minor      Must equal ZR_ENGINE_ABI_MINOR (0)
0x08    4     requested_engine_abi_patch      Must equal ZR_ENGINE_ABI_PATCH (0)

0x0C    4     requested_drawlist_version      Must equal ZR_DRAWLIST_VERSION_V1 (1)
0x10    4     requested_event_batch_version   Must equal ZR_EVENT_BATCH_VERSION_V1 (1)

0x14    36    limits                          zr_limits_t
0x38    8     plat                            plat_config_t

0x40    4     tab_width                        Tab stop width (default: 4)
0x44    4     width_policy                     See “Width policy” below
0x48    4     target_fps                       Scheduling hint (default: 60)

0x4C    1     enable_scroll_optimizations      0 or 1
0x4D    1     enable_debug_overlay             0 or 1
0x4E    1     enable_replay_recording          0 or 1
0x4F    1     _pad0                            Must be 0
```

### Width policy

`width_policy` is a pinned, deterministic switch used by Unicode width measurement:

| Value | Meaning |
|------:|---------|
| 0 | emoji width narrow (1 column) |
| 1 | emoji width wide (2 columns) |

## zr_engine_runtime_config_t (60 bytes)

This is the shape accepted by `engine_set_config()` (no version negotiation fields):

```
Offset  Size  Field                           Description
──────  ────  ─────                           ───────────
0x00    36    limits                          zr_limits_t
0x24    8     plat                            plat_config_t
0x2C    4     tab_width                        Tab stop width
0x30    4     width_policy                     See above
0x34    4     target_fps                       Scheduling hint
0x38    1     enable_scroll_optimizations      0 or 1
0x39    1     enable_debug_overlay             0 or 1
0x3A    1     enable_replay_recording          0 or 1
0x3B    1     _pad0                            Must be 0
```

## zr_limits_t (36 bytes)

Deterministic caps (validation rejects zeros):

```
Offset  Size  Field                     Description
──────  ────  ─────                     ───────────
0x00    4     arena_max_total_bytes      Arena total cap (bytes)
0x04    4     arena_initial_bytes        Arena initial size (bytes)
0x08    4     out_max_bytes_per_frame    Max bytes emitted by engine_present()

0x0C    4     dl_max_total_bytes         Max drawlist size (bytes)
0x10    4     dl_max_cmds                Max commands per drawlist
0x14    4     dl_max_strings             Max string spans
0x18    4     dl_max_blobs               Max blob spans
0x1C    4     dl_max_clip_depth          Max nested PUSH_CLIP depth
0x20    4     dl_max_text_run_segments   Max segments in a text-run blob
```

## plat_config_t (8 bytes)

Platform requests live in the OS-header-free `plat_config_t`:

```
Offset  Size  Field                    Description
──────  ────  ─────                    ───────────
0x00    1     requested_color_mode     PLAT_COLOR_MODE_UNKNOWN/16/256/RGB
0x01    1     enable_mouse             0 or 1
0x02    1     enable_bracketed_paste   0 or 1
0x03    1     enable_focus_events      0 or 1
0x04    1     enable_osc52             0 or 1
0x05    3     _pad                    Must be 0
```

## Version negotiation

`engine_create()` performs pinned version checks. In the current engine (ABI v1.0.0) the requested values must match exactly; mismatches return `ZR_ERR_UNSUPPORTED`.

## Example

```c
zr_engine_config_t cfg = zr_engine_config_default();

/* Optional: request RGB output and enable mouse input. */
cfg.plat.requested_color_mode = PLAT_COLOR_MODE_RGB;
cfg.plat.enable_mouse = 1;

/* Optional: raise caps for larger drawlists / output buffers. */
cfg.limits.dl_max_total_bytes = 64u * 1024u * 1024u;
cfg.limits.out_max_bytes_per_frame = 16u * 1024u * 1024u;

zr_engine_t* engine = NULL;
zr_result_t rc = engine_create(&engine, &cfg);
if (rc != ZR_OK) {
  /* handle error */
}
```
