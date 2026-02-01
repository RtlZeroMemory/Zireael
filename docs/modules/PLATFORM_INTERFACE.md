# Module — Platform Interface

The platform interface is the hard boundary between core determinism and OS-specific behavior.

## Rules

- Core includes only `src/platform/zr_platform.h` (OS-header-free).
- OS headers live only in `src/platform/posix` and `src/platform/win32`.
- Backends must provide a wake primitive to interrupt blocking waits.
- POSIX backend limitation (current): platform instances are a **process-wide singleton** because the SIGWINCH handler and wake write fd are stored in global state. A second `plat_create` must fail with `ZR_ERR_PLATFORM`.

See:

- `src/platform/zr_platform.h`
