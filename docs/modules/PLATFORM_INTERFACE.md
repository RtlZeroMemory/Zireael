# Module — Platform Interface

The platform interface is the hard boundary between core determinism and OS-specific behavior.

## Rules

- Core includes only `src/platform/zr_platform.h` (OS-header-free).
- OS headers live only in `src/platform/posix` and `src/platform/win32`.
- Backends must provide a wake primitive to interrupt blocking waits.

See:

- `src/platform/zr_platform.h`

