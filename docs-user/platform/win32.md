# Win32 Backend

The Win32 backend (`src/platform/win32/`) supports Windows 10+ with ConPTY/VT support.

## Responsibilities

- **Console setup**: Enable VT processing mode
- **Input**: Read console input records, convert to normalized events
- **Output**: Write VT sequences to console, single flush per `engine_present()`
- **Wake**: Event object for cross-thread wakeup

## Capabilities Negotiated

| Capability | Detection |
|------------|-----------|
| Color mode | Console mode flags, VT support check |
| Mouse | Enable mouse input mode |
| VT processing | ENABLE_VIRTUAL_TERMINAL_PROCESSING |

## Requirements

- Windows 10 version 1809 or later
- Console with VT processing support

## Console Restoration

On `engine_destroy()`:

1. Restore original console mode
2. Disable mouse input mode
3. Show cursor
4. Exit alternate screen (if entered)
