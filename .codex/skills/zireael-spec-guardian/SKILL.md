---
name: zireael-spec-guardian
description: Enforce MASTERDOC.MD constraints and repo guardrails for any Zireael engine change.
metadata:
  short-description: Spec + boundary compliance checklist
---

## When to use

Use this skill at the start of **any** task in this repo, especially when:

- adding/modifying ABI, formats, or platform code
- changing ownership/memory behavior
- changing Unicode, rendering, diff output, or tests

## Source of truth

- Treat `MASTERDOC.MD` as authoritative (locked).
- Internal architecture docs are in `docs/` (gitignored); use `docs/00_INDEX.md` for navigation if present.

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
- **Error model:** `0 = OK`, negative error codes for failures.
- **UB avoidance:** no type-punning through casts; drawlist parsing uses safe unaligned reads via `memcpy`; validate all bounds.
- **Hot paths:** avoid per-frame heap churn; use arenas; single buffered flush per `engine_present()`.

## Pre-flight checklist (before coding)

1. Identify which MASTERDOC sections apply (ABI, drawlist, events, unicode, diff, platform).
2. Identify affected modules and verify dependency direction:
   - util → unicode → core → platform interface → platform backends
3. Decide caps/limits impacted (drawlist size, cmd count, event queue depth, output bytes per frame, arena caps).
4. Decide test impact:
   - unit vs golden vs fuzz vs integration
   - determinism constraints (no wall clock / locale dependencies in unit tests)

## Review checklist (before finalizing)

- No OS headers added to `src/core|src/unicode|src/util`.
- No platform `#ifdef` leaked into core/unicode/util.
- No API returns pointers requiring caller free; all outputs use caller-provided buffers/structs.
- Parser paths (drawlist, input, UTF-8) have:
  - bounds checks
  - deterministic error returns
  - fuzz target considerations
- Diff output remains single-buffer flush per present; golden outputs updated intentionally if changed.
- `docs/` remains gitignored and uncommitted.

