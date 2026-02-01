---
name: zireael-build-test-ci
description: Maintain portable CMake builds across toolchains and a deterministic unit/golden/fuzz/integration testing strategy.
metadata:
  short-description: Build + CI + tests
---

## When to use

Use this skill when working on:

- `CMakeLists.txt` / `CMakePresets.json`
- CI configuration (GitHub Actions)
- warnings/sanitizers/toolchain coverage
- adding tests (unit/golden/fuzz/integration)

## Toolchains (locked intent)

- macOS: Apple Clang
- Linux: clang + gcc in CI
- Windows: clang-cl primary (+ optional mingw secondary)

## CMake guidance

- Always produce a static library (required).
- Shared library is optional/configurable.
- Tests must be runnable via CTest.
- In CI, build core/unicode/util with warnings-as-errors where feasible.

## Test strategy (locked intent)

- Unit tests: no OS, no terminal I/O; deterministic; run everywhere.
- Golden tests: byte-for-byte diff output fixtures; pin caps/policy/initial state.
- Fuzz tests: drawlist parser, input parser, UTF-8 decoder; no crash/hang; cap-respecting.
- Integration tests: PTY/ConPTY headless tests for raw mode lifecycle and platform behavior.

## Sanitizers

- Linux/macOS (clang): ASan + UBSan builds in CI.
- Windows: clang-cl sanitizers if feasible; otherwise rely on runtime checks + fuzz in nightly.

