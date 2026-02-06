# Module — Platform Interface

The platform interface is the hard boundary between core determinism and OS-specific behavior.

## Boundary rules

- Core includes only `src/platform/zr_platform.h` (OS-header-free)
- OS headers live only in `src/platform/posix/` and `src/platform/win32/`
- No `#ifdef` in core for platform differences

## API surface

Defined in `src/platform/zr_platform.h`.

### Lifecycle

| Function | Description |
|----------|-------------|
| `plat_create(out, cfg)` | Create platform instance |
| `plat_destroy(plat)` | Destroy platform instance |

### Raw mode

| Function | Description |
|----------|-------------|
| `plat_enter_raw(plat)` | Enter raw mode (idempotent) |
| `plat_leave_raw(plat)` | Leave raw mode (idempotent) |

Raw mode enables unbuffered input and disables terminal echo/line editing.

### Capabilities and size

| Function | Description |
|----------|-------------|
| `plat_get_size(plat, out)` | Get terminal dimensions (cols, rows) |
| `plat_get_caps(plat, out)` | Get detected capabilities |

### I/O

| Function | Description |
|----------|-------------|
| `plat_read_input(plat, buf, cap)` | Read available input bytes (non-blocking) |
| `plat_write_output(plat, bytes, len)` | Write output bytes to terminal |

### Wait/wake

| Function | Returns | Description |
|----------|---------|-------------|
| `plat_wait(plat, timeout_ms)` | 1=ready, 0=timeout, <0=error | Block until input ready or timeout |
| `plat_wake(plat)` | `ZR_OK` or error | Wake blocked wait (thread-safe) |

`plat_wake` is the only function callable from non-engine threads.

### Time

| Function | Description |
|----------|-------------|
| `plat_now_ms()` | Monotonic time in milliseconds |

## Output capabilities

`plat_get_caps()` reports terminal/backend capabilities that affect output emission:

- `plat_caps_t.supports_scroll_region` — safe to use DECSTBM + SU/SD for scroll optimizations
- `plat_caps_t.supports_sync_update` — safe to wrap presents in DEC private mode `?2026` (synchronized updates)
- `plat_caps_t.supports_cursor_shape` — safe to emit DECSCUSR (`ESC[Ps q`) for cursor shape/blink control
- `plat_caps_t.supports_output_wait_writable` — backend supports `plat_wait_output_writable()` for bounded output pacing

## Output backpressure hook

To support optional frame pacing, the platform interface exposes an output-writability wait:

- `plat_wait_output_writable(plat_t* plat, int32_t timeout_ms)`:
  - returns `ZR_OK` when output is writable within `timeout_ms`
  - returns `ZR_ERR_LIMIT` on timeout
  - returns `ZR_ERR_UNSUPPORTED` when the backend cannot support writability waits
  - returns `ZR_ERR_PLATFORM` on OS failures

## Backend constraints

### POSIX

- Process-wide singleton: SIGWINCH handler and wake fd are global state
- Second `plat_create` fails with `ZR_ERR_PLATFORM`
- Uses self-pipe for signal-safe wake
- Chains to any previously installed `SIGWINCH` handler and restores it on destroy
- `plat_wait_output_writable`: uses `poll(POLLOUT)` on stdout fd

### Windows

- Uses ConPTY or native console API
- Multiple engines may be creatable but contend for console
- Treat as single-engine unless you fully control console ownership
- Key input text is translated to UTF-8 bytes before core parsing
- `plat_wait_output_writable`: best-effort; returns `ZR_ERR_UNSUPPORTED` if not feasible

## Implementation files

- `src/platform/zr_platform.h` — OS-header-free interface
- `src/platform/posix/zr_plat_posix.c` — POSIX backend
- `src/platform/win32/zr_plat_win32.c` — Windows backend
