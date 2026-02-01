# Module: Platform interface

The platform interface is the hard boundary between the deterministic core and
OS-specific behavior.

## Source of truth

- Boundary header (OS-header-free): `src/platform/zr_platform.h`
- Implementations: `src/platform/posix/`, `src/platform/win32/`
- Internal spec (normative): `docs/modules/PLATFORM_INTERFACE.md`

## Rules

- `src/core`, `src/unicode`, `src/util` must not include OS headers.
- OS code lives only in platform backends.
- Backends provide a wake primitive to interrupt blocking waits.
