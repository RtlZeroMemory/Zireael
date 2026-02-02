# Win32 Backend

The Win32 backend (`src/platform/win32/`) supports Windows 10+ with ConPTY/VT support.

## Responsibilities

- **Console setup**: Enable VT processing mode
- **Input**: Read VT input bytes (ENABLE_VIRTUAL_TERMINAL_INPUT) and parse into normalized events
- **Output**: Write VT sequences to console, single flush per `engine_present()`
- **Wake**: Event object for cross-thread wakeup

## Capabilities Negotiated

The v1 Win32 backend reports a conservative, deterministic caps model:

- `plat_caps_t.color_mode` is set to the requested mode (`plat_config_t.requested_color_mode`).
- The `supports_*` fields are currently reported as supported (1) when enabled by config.

VT input/output is validated on `plat_enter_raw()`. If VT input cannot be enabled, `engine_create()` / raw enter may fail with `ZR_ERR_UNSUPPORTED`.

## Requirements

- Windows 10 version 1809 or later
- Console with VT processing support

## Console Restoration

On `engine_destroy()`:

1. Restore original console mode
2. Disable mouse input mode
3. Show cursor
4. Exit alternate screen (if entered)
