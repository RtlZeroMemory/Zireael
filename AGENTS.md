# Zireael — Agent Guide (Repo-Wide)

This repository is the **C core engine only**. A TypeScript wrapper/product will live in a separate repo later.

## Source of truth (locked)

- `README.md` — GitHub-facing overview (architecture, boundary rules, build/test intent).
- `docs/00_INDEX.md` — internal reading path and normative module specs under `docs/`.

If any file, comment, or suggestion conflicts with the normative docs set (`docs/`), defer to `docs/`.

## Internal docs (not committed)

Implementation-ready internal docs live under `docs/` and are **gitignored**. Start here:

- `docs/00_INDEX.md` (reading path)
- `docs/SAFETY_RULESET.md` (LOCKED Safe C rulebook)
- `docs/LIBC_POLICY.md` (LOCKED libc allow/forbid policy)
- `docs/ERROR_CODES_CATALOG.md` (LOCKED single error/result catalog)
- `docs/VERSION_PINS.md` (LOCKED version/policy pins for determinism)
- `docs/GOLDEN_FIXTURE_FORMAT.md` (LOCKED golden fixture format, byte-for-byte comparison)
- `docs/CODE_STANDARDS.md`, `docs/REPO_LAYOUT.md`, `docs/BUILD_TOOLCHAINS_AND_CMAKE.md` (normative engineering practices)
- `docs/EPIC_PLAN.md` (EPIC/task-ready plan)
- `docs/modules/` (full module architecture specs)

Do **not** stage/commit anything under `docs/`.

## Non-negotiables (quick checklist)

- Engine-only repo: **no TypeScript**, no Node tooling, no monorepo structure.
- Hard platform boundary:
  - `src/core`, `src/unicode`, `src/util` **MUST NOT** include OS headers.
  - All OS code must live in `src/platform/win32` and `src/platform/posix`.
  - Avoid `#ifdef` in core/unicode/util (allowed only in platform backends and minimal build-selection glue).
- Ownership model is locked:
  - engine owns its allocations; caller never frees engine memory
  - caller provides drawlist bytes and event output buffers
- Error model: `0 = OK`, negative codes for failures.
- Hot paths: no per-frame heap churn; buffer output; single flush per present.
- Safe C: follow `docs/SAFETY_RULESET.md` and `docs/LIBC_POLICY.md`.
- Documentation + comments:
  - every `.c`/`.h` file MUST have a brief top-of-file “what/why” header comment
  - code changes that affect behavior/ABI/formats/invariants MUST update internal docs in the same change
  - prefer expressive naming + small focused functions; add comments only when needed for correctness

## Repo layout (high level)

- `src/core/` — engine loop, event queue, framebuffer, diff, drawlist execution
- `src/unicode/` — UTF-8 decode, graphemes, width, wrapping/measurement
- `src/util/` — arena, vec, ring, string helpers, logging, asserts, caps
- `src/platform/win32/` — Windows console backend (VT enable, input, resize, wake)
- `src/platform/posix/` — POSIX backend (termios raw, poll/select, signals, wake)
- `tests/` — `unit/`, `golden/`, `integration/`, `fuzz/`
- `examples/` — small C examples only (no wrapper code)

## Build + test (scaffold)

- Configure/build via presets in `CMakePresets.json`.
- Tests run via CTest (`ctest --output-on-failure`) once implementation lands.

## Codex skills (repo-scoped)

This repo defines project skills under `.codex/skills/` (checked in).

- Skill index and usage: `SKILLS.md`
- Invoke in Codex by typing `$<skill-name>` (or use `/skills` to select).

Recommended starting skill for any task: `$zireael-spec-guardian`.

## Quick navigation

- Overview: `README.md`
- Internal docs index: `docs/00_INDEX.md`
- Skills index: `SKILLS.md`
- Build: `CMakeLists.txt`, `CMakePresets.json`
- Core modules: `src/core/`, `src/unicode/`, `src/util/`
- Platform backends: `src/platform/win32/`, `src/platform/posix/`
- Tests: `tests/unit/`, `tests/golden/`, `tests/fuzz/`, `tests/integration/`
