# ABI Overview

Zireael exposes a stable binary interface for FFI bindings. This section documents everything needed to integrate from any language.

## Data Flow

```
Wrapper                          Engine
───────                          ──────

  ┌─────────────┐
  │ Build       │
  │ drawlist    │ ─── bytes ───▶ engine_submit_drawlist()
  │ (binary)    │                  │
  └─────────────┘                  ▼
                                 ┌─────────────┐
                                 │ Validate    │
                                 │ Execute     │
                                 │ Framebuffer │
                                 └─────────────┘
                                   │
  engine_present() ◀───────────────┘
                                   │
                                   ▼
                                 ┌─────────────┐
                                 │ Diff        │
                                 │ Render      │
                                 │ Terminal    │
                                 └─────────────┘

  ┌─────────────┐
  │ Parse       │
  │ event batch │ ◀── bytes ─── engine_poll_events()
  │ (binary)    │
  └─────────────┘
```

## Binary Format Rules

| Rule | Value |
|------|-------|
| Byte order | Little-endian |
| Alignment | 4-byte aligned records |
| Reserved fields | Must be zero |
| Magic values | ASCII as little-endian u32 |

## Versioned Formats

| Format | Version | Magic (hex) | Magic (ASCII) |
|--------|---------|-------------|---------------|
| Drawlist | 1 | `0x4C44525A` | "ZRDL" |
| Event Batch | 1 | `0x5645525A` | "ZREV" |

## Error Codes

All functions return `zr_result_t` (int32):

| Code | Value | Meaning |
|------|-------|---------|
| `ZR_OK` | 0 | Success |
| `ZR_ERR_INVALID_ARGUMENT` | -1 | Null pointer or invalid parameter |
| `ZR_ERR_OOM` | -2 | Out of memory |
| `ZR_ERR_LIMIT` | -3 | Exceeded configured limit |
| `ZR_ERR_UNSUPPORTED` | -4 | Unknown version or feature |
| `ZR_ERR_FORMAT` | -5 | Malformed binary data |
| `ZR_ERR_PLATFORM` | -6 | Platform/OS error |

Exception: `engine_poll_events()` returns bytes written (>= 0) on success.

## Engine Functions

### engine_create

```c
zr_result_t engine_create(zr_engine_t** out_engine, const zr_engine_config_t* cfg);
```

Creates engine instance. Takes ownership of terminal I/O (raw mode).

## Platform Constraints

- **POSIX:** The backend installs a process-global SIGWINCH handler and uses a global wake fd. Only one active engine is allowed; a second `engine_create()` fails with `ZR_ERR_PLATFORM`.
- **Windows:** Multiple engines may be creatable, but they will contend for the same console. Treat the process as “single active engine” unless you fully control stdio and console mode ownership.

### engine_destroy

```c
void engine_destroy(zr_engine_t* e);
```

Destroys engine, restores terminal state.

### engine_poll_events

```c
int engine_poll_events(zr_engine_t* e, int timeout_ms, uint8_t* out_buf, int out_cap);
```

Polls for input. Returns bytes written or negative error.

- `timeout_ms = 0`: Non-blocking
- `timeout_ms > 0`: Wait up to N milliseconds
- `timeout_ms < 0`: Invalid (`ZR_ERR_INVALID_ARGUMENT`)

### engine_submit_drawlist

```c
zr_result_t engine_submit_drawlist(zr_engine_t* e, const uint8_t* bytes, int len);
```

Validates and executes drawlist into framebuffer. No partial effects on error.

### engine_present

```c
zr_result_t engine_present(zr_engine_t* e);
```

Diffs framebuffer, emits terminal output. Single write per call.

### engine_post_user_event

```c
zr_result_t engine_post_user_event(zr_engine_t* e, uint32_t tag,
                                    const uint8_t* payload, int len);
```

Posts user event from another thread. **Only thread-safe function.**

### engine_get_metrics / engine_set_config

Runtime introspection and reconfiguration.

## Threading

- `engine_post_user_event()` is safe to call from another thread to wake an engine blocked in `engine_poll_events()`.
- All other functions are engine-thread only (treat the engine as not thread-safe).

## Next

- [Drawlist v1](drawlist-v1.md) — Binary format for rendering commands
- [Event Batch v1](event-batch-v1.md) — Binary format for input events
- [Configuration](config.md) — Config struct layout
