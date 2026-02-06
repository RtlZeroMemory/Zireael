# Contributing

Zireael is a deterministic C engine with strict ABI, safety, and platform-boundary rules.

This guide describes the minimum quality bar for contributions.

## Ground Rules

1. Platform boundary is strict:
   - OS headers/code only in `src/platform/posix` and `src/platform/win32`
   - no OS headers in `src/core`, `src/unicode`, `src/util`
2. Treat all external bytes as untrusted.
3. Preserve error contract (`ZR_OK == 0`, failures negative, no partial effects on failure).
4. Avoid per-frame heap churn in hot paths.
5. Keep wrapper-facing and internal docs synchronized when behavior changes.

## Source Of Truth

- Internal normative docs: `docs/` (start: `docs/00_INDEX.md`)
- Public API headers: `include/zr/`

If implementation and internal docs disagree, internal docs win.

## Development Setup

```bash
git clone https://github.com/RtlZeroMemory/Zireael.git
cd Zireael

cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
ctest --preset posix-clang-debug --output-on-failure

bash scripts/guardrails.sh
python3 scripts/check_version_pins.py
```

Windows flow:

```powershell
.\scripts\vsdev.ps1
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
ctest --preset windows-clangcl-debug --output-on-failure
```

## Documentation Checks

```bash
bash scripts/docs.sh build
```

This runs strict MkDocs and generates Doxygen API docs when available.

## Release And Versioning Model

- Follow SemVer release tags: `vMAJOR.MINOR.PATCH` or `vMAJOR.MINOR.PATCH-<pre>`
- Keep `include/zr/zr_version.h` and `CHANGELOG.md` aligned before tagging
- See `docs/release-model.md` for release channels and policy details

Release validation:

```bash
python3 scripts/check_release_tag.py vX.Y.Z
```

## Code Style and Safety

Required references:

- `docs/CODE_STANDARDS.md`
- `docs/SAFETY_RULESET.md`
- `docs/LIBC_POLICY.md`

Key expectations:

- top-of-file "what/why" comments on `.c`/`.h`
- comments explain intent/constraints, not obvious syntax
- named constants instead of unexplained magic numbers
- bounds-safe parsing for all binary inputs

## Pull Request Checklist

Before opening a PR:

- relevant tests pass (`ctest ...`)
- guardrails pass
- version pin checks pass
- docs updated for ABI/format/behavior changes
- changelog updated for user-visible changes

In PR description, include:

- what changed
- why it changed
- risk/compatibility notes
- test evidence

## What We Accept

- bug fixes with regression coverage
- deterministic performance improvements
- ABI-safe feature additions aligned with roadmap
- docs and tooling improvements that reduce integration risk

## What We Reject

- breaking ABI changes without explicit versioning policy updates
- boundary violations (OS headers in core/unicode/util)
- unsafe parsing or unchecked bounds arithmetic
- behavior changes without docs/spec updates
