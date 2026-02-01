---
name: zireael-spec-guardian
description: Enforce Zireael’s locked docs, boundary rules, and safety guardrails for any change.
metadata:
  short-description: Spec + boundary compliance checklist
---

## When to use

Use this skill at the start of **any** task in this repo, especially when:

- adding/modifying ABI, formats, or platform code
- changing ownership/memory behavior
- changing Unicode, rendering, diff output, or tests

## Source of truth

- GitHub-facing overview: `README.md`
- Normative internal specs (implementation rules and module docs): `docs/00_INDEX.md` and `docs/**`

Key locked docs:

- `docs/SAFETY_RULESET.md`
- `docs/LIBC_POLICY.md`
- `docs/ERROR_CODES_CATALOG.md`
- `docs/VERSION_PINS.md`
- `docs/GOLDEN_FIXTURE_FORMAT.md`

## Hard constraints (must not violate)

- **Engine-only repo:** do not add TypeScript, Node tooling, or monorepo structure.
- **Platform boundary:**
  - `src/core`, `src/unicode`, `src/util` must not include OS headers.
  - OS-specific code only in `src/platform/win32` and `src/platform/posix`.
  - `#ifdef _WIN32` allowed only in platform backends and minimal build-selection glue.
- **Ownership model (locked):**
  - engine owns all allocations it makes; caller never frees engine memory
  - caller provides drawlist bytes and event output buffers
  - engine must not return heap pointers requiring caller free
- **Error model:** `0 = OK`, negative error codes for failures (`ZR_ERR_*`).
- **UB avoidance:** no type-punning through casts; drawlist parsing uses safe unaligned reads via `memcpy`; validate all bounds.
- **Hot paths:** avoid per-frame heap churn; use arenas; single buffered flush per `engine_present()`.

## Pre-flight checklist (before coding)

1. Identify affected docs/modules (`docs/00_INDEX.md` reading order).
2. Verify dependency direction:
   - util → unicode → core → platform interface → platform backends
3. Verify version pins and defaults (`docs/VERSION_PINS.md`), especially:
   - Unicode version + width policy
   - drawlist/event batch versions
4. Decide caps/limits impacted (`zr_limits_t`) and the expected failure code (`ZR_ERR_LIMIT` vs `ZR_ERR_OOM`).
5. Decide test impact:
   - unit vs golden vs fuzz vs integration
   - determinism constraints (no locale/wall-clock in unit tests)

## Review checklist (before finalizing)

- No OS headers added to `src/core|src/unicode|src/util`.
- No platform `#ifdef` leaked into core/unicode/util.
- No API returns pointers requiring caller free; all outputs use caller-provided buffers/structs.
- Error returns are consistent with `docs/ERROR_CODES_CATALOG.md` (and “no partial effects” is preserved unless explicitly allowed).
- Parser paths (drawlist, input, UTF-8) have:
  - bounds checks
  - deterministic error returns
  - fuzz target considerations
- Diff output remains single-buffer flush per present; golden outputs updated intentionally if changed.
- If behavior/ABI/formats/invariants change, update the corresponding `docs/**` in the same change.
