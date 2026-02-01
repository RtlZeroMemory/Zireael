# Zireael — Agent Guide (Repo-Wide)

This repository is the **C core engine only**. A TypeScript wrapper/product will live in a separate repo later.

## Source of truth (locked)

- `README.md` — GitHub-facing overview (architecture, boundary rules, build/test intent)
- `docs/00_INDEX.md` — internal reading path and normative module specs under `docs/`

If any file, comment, or suggestion conflicts with the normative docs set (`docs/`), defer to `docs/`.

## Docs (normative)

Implementation-ready internal docs live under `docs/` and are checked in. Start here:

- `docs/00_INDEX.md` (reading path)
- `docs/CODE_STANDARDS.md` (style, comments, naming — **read this for code quality**)
- `docs/SAFETY_RULESET.md` (LOCKED Safe C rulebook)
- `docs/LIBC_POLICY.md` (LOCKED libc allow/forbid policy)
- `docs/ERROR_CODES_CATALOG.md` (LOCKED single error/result catalog)
- `docs/VERSION_PINS.md` (LOCKED version/policy pins for determinism)
- `docs/GOLDEN_FIXTURE_FORMAT.md` (LOCKED golden fixture format)
- `docs/REPO_LAYOUT.md`, `docs/BUILD_TOOLCHAINS_AND_CMAKE.md` (engineering practices)
- `docs/modules/` (full module architecture specs)

## Non-negotiables (quick checklist)

- Engine-only repo: **no TypeScript**, no Node tooling, no monorepo structure.
- Hard platform boundary:
    - `src/core`, `src/unicode`, `src/util` **MUST NOT** include OS headers.
    - All OS code must live in `src/platform/win32` and `src/platform/posix`.
    - Avoid `#ifdef` in core/unicode/util (allowed only in platform backends).
- Ownership model is locked:
    - engine owns its allocations; caller never frees engine memory
    - caller provides drawlist bytes and event output buffers
- Error model: `0 = OK`, negative codes for failures.
- Hot paths: no per-frame heap churn; buffer output; single flush per present.
- Safe C: follow `docs/SAFETY_RULESET.md` and `docs/LIBC_POLICY.md`.

## Code style and comments

Follow `docs/CODE_STANDARDS.md`. Key points:

- Every `.c`/`.h` file MUST have a top-of-file "what/why" header comment
- Comments explain **why**, not **what** — don't restate code
- Extract magic numbers to named constants with descriptive names
- Functions should be 20-40 lines, max 50 lines
- Use `!ptr` for NULL checks consistently
- Add ASCII diagrams for complex data structures (ring buffers, etc.)
- Add section markers in long functions (`/* --- Section Name --- */`)

See `.codex/skills/zireael-code-style/SKILL.md` for detailed guidance.

## Repo layout (high level)

- `src/core/` — engine loop, event queue, framebuffer, diff, drawlist execution
- `src/unicode/` — UTF-8 decode, graphemes, width, wrapping/measurement
- `src/util/` — arena, vec, ring, string helpers, logging, asserts, caps
- `src/platform/win32/` — Windows console backend
- `src/platform/posix/` — POSIX backend
- `tests/` — `unit/`, `golden/`, `integration/`, `fuzz/`
- `examples/` — small C examples only

## Build + test

- Configure/build via presets in `CMakePresets.json`
- Tests run via CTest (`ctest --output-on-failure`)
- Windows (clang-cl): run `.\scripts\vsdev.ps1` before cmake

## Codex skills (repo-scoped)

This repo defines project skills under `.codex/skills/` (checked in).

- Skill index: `SKILLS.md`
- Read the relevant skill file before starting domain-specific work

**Recommended skills by task:**

| Task                  | Skill to Read                                       |
|-----------------------|-----------------------------------------------------|
| Any task (start here) | `.codex/skills/zireael-spec-guardian/SKILL.md`      |
| Code style/comments   | `.codex/skills/zireael-code-style/SKILL.md`         |
| Platform code         | `.codex/skills/zireael-platform-boundary/SKILL.md`  |
| Unicode/text          | `.codex/skills/zireael-unicode-text/SKILL.md`       |
| ABI/formats           | `.codex/skills/zireael-abi-formats/SKILL.md`        |
| Error handling        | `.codex/skills/zireael-error-contracts/SKILL.md`    |
| Diff renderer         | `.codex/skills/zireael-rendering-diff/SKILL.md`     |
| Golden tests          | `.codex/skills/zireael-golden-fixtures/SKILL.md`    |
| Headers/includes      | `.codex/skills/zireael-header-layering/SKILL.md`    |
| Build/CI/tests        | `.codex/skills/zireael-build-test-ci/SKILL.md`      |

## Quick navigation

- Overview: `README.md`
- Internal docs index: `docs/00_INDEX.md`
- Skills index: `SKILLS.md`
- Code style: `docs/CODE_STANDARDS.md`
- Build: `CMakeLists.txt`, `CMakePresets.json`
- Core modules: `src/core/`, `src/unicode/`, `src/util/`
- Platform backends: `src/platform/win32/`, `src/platform/posix/`
- Tests: `tests/unit/`, `tests/golden/`, `tests/fuzz/`, `tests/integration/`
