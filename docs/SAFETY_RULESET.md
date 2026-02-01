# Zireael — Safety Ruleset (Locked)

This document defines the project’s non-negotiable “Safe C” subset and determinism rules.

## Core goals

- No undefined behavior (UB).
- Deterministic behavior across platforms/toolchains.
- Clear ownership: the engine owns its allocations; callers never free engine memory.
- Parser safety: no out-of-bounds reads/writes and no type-punning.

## Required practices

- Validate arguments at public API boundaries; return `ZR_ERR_INVALID_ARGUMENT` for invalid inputs.
- Prefer “validate fully before mutating state” to preserve “no partial effects” on failure.
- For multi-resource functions, use a single `cleanup:` block (`goto cleanup`) and make failure paths obvious.
- Use checked integer math helpers (`zr_checked_*`) when computing sizes/offsets for parsing and writes.
- Unaligned reads MUST use byte loads or `memcpy` (no casts to wider integer types).
- Out-parameters MUST be either:
  - fully written on success and left in a documented safe state on failure, or
  - explicitly zeroed at the start of the function.

## Forbidden practices

- Type-punning through casts.
- Reading past provided buffer lengths.
- Locale-dependent behavior (no `setlocale`, no `wcwidth`, etc.).
- Wall-clock dependencies in unit tests (tests must be deterministic).
- OS headers in `src/core/`, `src/unicode/`, or `src/util/`.

See also:

- `docs/CODE_STANDARDS.md`
- `docs/LIBC_POLICY.md`
- `docs/ERROR_CODES_CATALOG.md`

