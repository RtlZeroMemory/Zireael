# Platform support

Zireael has a strict platform boundary:

- Core logic, Unicode, and utilities live under `src/core`, `src/unicode`, `src/util` and must remain OS-header-free.
- Platform backends live under `src/platform/posix` and `src/platform/win32`.

## POSIX

The POSIX backend uses terminals via `termios` and reads/writes bytes on file descriptors.

## Windows

The Win32 backend uses the Windows console APIs (including ConPTY where applicable).

## Next steps

- [Internal Specs → Platform Interface](../modules/PLATFORM_INTERFACE.md)
- [Dev → Architecture](../dev/architecture.md)

