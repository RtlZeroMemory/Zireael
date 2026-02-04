# Zireael — Header Layering (Normative)

This document defines include layering rules and the responsibilities of public/ABI headers.

## Include layering (locked)

Dependency direction (allowed `#include` flow):

```
src/util/      → OS-header-free; may include only other util headers + C std headers
src/unicode/   → OS-header-free; may include util + C std headers
src/core/      → OS-header-free; may include util, unicode, and src/platform/zr_platform.h
src/platform/  → platform interface (`zr_platform.h`) is OS-header-free; backends may include OS headers
```

Rules:

- `src/util/**`, `src/unicode/**`, `src/core/**` MUST NOT include OS headers.
- OS headers live only in `src/platform/posix/**` and `src/platform/win32/**`.
- Platform `#ifdef` is allowed only in platform backends and platform selection translation units.
- Core code may include only the OS-header-free platform boundary: `src/platform/zr_platform.h`.

## Public header responsibilities

### `include/zr/zr_version.h`

- Pure pinned constants/macros for ABI negotiation and binary formats.
- MUST remain OS-header-free and free of runtime logic.

### `include/zr/zr_config.h`

- Defines `zr_engine_config_t` and `zr_engine_runtime_config_t`.
- Declares deterministic defaults and validation entrypoints.
- MUST NOT expose ownership that requires the caller to free engine memory.

### `include/zr/zr_engine.h`

- Primary public ABI entrypoints.
- Exposes only an opaque `zr_engine_t` handle; callers do not know the engine layout.
- Documents thread rules and ownership rules (see `docs/SAFETY_RULESET.md`).

### `src/platform/zr_platform.h`

- OS-header-free platform boundary interface used by `src/core/**`.
- Backends implement this interface in `src/platform/posix/**` and `src/platform/win32/**`.
