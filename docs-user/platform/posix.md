# POSIX Backend

The POSIX backend (`src/platform/posix/`) supports Linux and macOS.

## Responsibilities

- **Terminal setup**: Raw mode via termios, disable echo and canonical mode
- **Input**: Read from stdin; handle SIGWINCH for resize
- **Output**: Write to stdout, single flush per `engine_present()`
- **Wake**: Self-pipe for cross-thread wakeup

## Capabilities Negotiated

The v1 POSIX backend exposes a conservative, deterministic caps model:

- `plat_caps_t.color_mode` is set to the requested mode (`plat_config_t.requested_color_mode`).
- The boolean `supports_*` fields are currently reported as supported (1) when the feature is enabled by config.

Wrappers should treat caps as informational; the engine always deterministically clamps output to the negotiated policy.

## Limitations

- **Single active engine**: SIGWINCH wake uses process-global state; a second `engine_create()` fails with `ZR_ERR_PLATFORM`.
- **Signal handling**: SIGWINCH is installed while the engine is active and may conflict with application handlers.

## Terminal Restoration

On `engine_destroy()`:

1. Restore original termios
2. Disable mouse mode
3. Show cursor
4. Exit alternate screen (if entered)
