# Platform Support

Zireael enforces a strict architectural boundary between deterministic core logic and OS-specific terminal I/O.

## Boundary Rules

- `src/core`, `src/unicode`, `src/util` are OS-header-free.
- OS-specific code is isolated to:
  - `src/platform/posix`
  - `src/platform/win32`
- Core includes only platform abstraction headers, not OS APIs directly.

## POSIX Backend

Typical mechanisms:

- terminal raw mode via `termios`
- file-descriptor reads/writes
- wait/wake integration for polling

## Windows Backend

Typical mechanisms:

- Windows console APIs / ConPTY paths where applicable
- backend capability detection translated into platform-neutral caps

## Capability Snapshot

Wrappers can query runtime capabilities via `engine_get_caps()`.

Examples include:

- color mode
- mouse/bracketed-paste support
- synchronized update support
- scroll-region support
- cursor-shape support

## Portability Guidance For Wrappers

- gate optional behaviors on runtime caps, not assumptions
- avoid backend-specific parsing in wrappers when engine already normalizes events
- use event/drawlist version pins explicitly

## Next Steps

- [Internal -> Platform Interface](../modules/PLATFORM_INTERFACE.md)
- [Dev -> Architecture](../dev/architecture.md)
- [ABI -> C ABI Reference](../abi/c-abi-reference.md)
