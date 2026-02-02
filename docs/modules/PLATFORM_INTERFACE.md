# Module — Platform Interface

The platform interface is the hard boundary between core determinism and OS-specific behavior.

## Rules

- Core includes only `src/platform/zr_platform.h` (OS-header-free).
- OS headers live only in `src/platform/posix` and `src/platform/win32`.
- Backends must provide a wake primitive to interrupt blocking waits.
- POSIX backend limitation (current): platform instances are a **process-wide singleton** because the SIGWINCH handler
  and wake write fd are stored in global state. A second `plat_create` must fail with `ZR_ERR_PLATFORM`.

See:

- `src/platform/zr_platform.h`

## Output capabilities

`plat_get_caps()` reports terminal/backend capabilities that affect output emission:

- `plat_caps_t.supports_scroll_region` — safe to use DECSTBM + SU/SD for scroll optimizations.
- `plat_caps_t.supports_sync_update` — safe to wrap presents in DEC private mode `?2026` (synchronized updates).
- `plat_caps_t.supports_cursor_shape` — safe to emit DECSCUSR (`ESC[Ps q`) for cursor shape/blink control.
- `plat_caps_t.supports_output_wait_writable` — backend supports `plat_wait_output_writable()` for bounded output pacing.

## Output backpressure hook

To support optional frame pacing, the platform interface exposes an output-writability wait:

- `plat_wait_output_writable(plat_t* plat, int32_t timeout_ms)`:
  - returns `ZR_OK` when output is writable within `timeout_ms`
  - returns `ZR_ERR_LIMIT` on timeout
  - returns `ZR_ERR_UNSUPPORTED` when the backend cannot support writability waits
  - returns `ZR_ERR_PLATFORM` on OS failures

Backends should implement this conservatively:

- POSIX: `poll(POLLOUT)` on stdout fd with the given timeout.
- Win32: best-effort; if not feasible, return `ZR_ERR_UNSUPPORTED` and report that lack of support via caps.
