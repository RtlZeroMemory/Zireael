# Zireael — Agent Guide (Repo-Wide)

This repository is the **C core engine only**. A TypeScript wrapper/product will live in a separate repo later.

## Single source of truth (locked)

- `MASTERDOC.MD` — **must** be treated as authoritative for requirements, ABI, portability rules, ownership model, and testing intent.

If any file, comment, or suggestion conflicts with `MASTERDOC.MD`, defer to `MASTERDOC.MD`.

## Internal docs (not committed)

Implementation-ready internal docs live under `docs/` and are **gitignored**. Use:

- `docs/00_INDEX.md` (reading order + TOC)
- `docs/01_OVERVIEW.md` (high-level architecture + data flow)
- `docs/02_REPO_LAYOUT.md` (module boundaries + `#ifdef` rules)
- `docs/04_CODE_STANDARDS.md` (Safe C Rule Set + stdlib whitelist + comment/doc rules)
- `docs/05_ALLOCATOR_OWNERSHIP.md` (locked ownership + arenas)
- `docs/06_EVENT_SYSTEM.md` (normalized events + packed event ABI)
- `docs/07_DRAWLIST_FORMAT.md` (binary drawlist ABI)
- `docs/09_UNICODE_TEXT.md` (Unicode decisions + primitives)
- `docs/10_DIFF_RENDERER.md` (diff pipeline + golden hooks)
- `docs/11_PLATFORM_LAYER.md` (POSIX/Win32 backend notes)
- `docs/14_MODULE_CATALOG.md` (planned files/modules for epics/tasks)

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
- Safe C: follow `docs/04_CODE_STANDARDS.md` (MUST/MUST NOT rules + stdlib whitelist).
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

- Specs: `MASTERDOC.MD`
- Skills index: `SKILLS.md`
- Build: `CMakeLists.txt`, `CMakePresets.json`
- Core modules: `src/core/`, `src/unicode/`, `src/util/`
- Platform backends: `src/platform/win32/`, `src/platform/posix/`
- Tests: `tests/unit/`, `tests/golden/`, `tests/fuzz/`, `tests/integration/`
