---
name: zireael-spec-guardian
description: Enforce locked docs, boundary rules, and safety guardrails for any Zireael change.
metadata:
  short-description: Spec + boundary compliance
---

## When to use

**Use this skill first** for any task in this repo, especially when:

- adding/modifying ABI, formats, or platform code
- changing ownership/memory behavior
- changing Unicode, rendering, diff output, or tests

## Source of truth

- `README.md` — GitHub-facing overview
- `docs/00_INDEX.md` — internal reading path
- `docs/CODE_STANDARDS.md` — code style and comments

Key locked docs:

- `docs/SAFETY_RULESET.md`
- `docs/LIBC_POLICY.md`
- `docs/ERROR_CODES_CATALOG.md`
- `docs/VERSION_PINS.md`
- `docs/GOLDEN_FIXTURE_FORMAT.md`

## Hard constraints (must not violate)

- **Engine-only repo:** no TypeScript, Node tooling, or monorepo
- **Platform boundary:**
    - `src/core`, `src/unicode`, `src/util` MUST NOT include OS headers
    - OS code only in `src/platform/win32/` and `src/platform/posix/`
    - `#ifdef _WIN32` only in platform backends
- **Ownership (locked):**
    - engine owns all its allocations
    - caller never frees engine memory
    - engine doesn't return heap pointers requiring caller free
- **Error model:** `0 = OK`, negative `ZR_ERR_*` codes
- **UB avoidance:** no type-punning; safe unaligned reads; validate all bounds
- **Hot paths:** no per-frame heap churn; single flush per present

## Pre-flight checklist

1. Identify affected docs (`docs/00_INDEX.md`)
2. Verify dependency direction: util → unicode → core → platform
3. Check version pins (`docs/VERSION_PINS.md`)
4. Decide caps/limits impacted (`zr_limits_t`)
5. Decide test impact (unit/golden/fuzz/integration)

## Review checklist

- [ ] No OS headers in core/unicode/util
- [ ] No `#ifdef` leaked into core/unicode/util
- [ ] No API returns pointers requiring caller free
- [ ] Error returns match `docs/ERROR_CODES_CATALOG.md`
- [ ] Parsers have bounds checks and fuzz consideration
- [ ] Golden outputs updated if behavior changed
- [ ] Docs updated if behavior/ABI/formats changed
