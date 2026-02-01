---
name: zireael-build-test-ci
description: Maintain portable CMake builds and deterministic unit/golden/fuzz/integration tests.
metadata:
  short-description: Build + CI + tests
---

## When to use

Use this skill when working on:

- `CMakeLists.txt` or `CMakePresets.json`
- CI configuration (GitHub Actions)
- warnings/sanitizers/toolchain setup
- adding tests (unit/golden/fuzz/integration)

## Source of truth

- `docs/BUILD_TOOLCHAINS_AND_CMAKE.md` — build spec
- `docs/modules/TESTING_GOLDENS_FUZZ_INTEGRATION.md` — test strategy
- `docs/GOLDEN_FIXTURE_FORMAT.md` — fixture format

## Toolchains

- macOS: Apple Clang
- Linux: Clang + GCC in CI
- Windows: clang-cl primary

## CMake guidance

- Always produce static library (required)
- Shared library optional/configurable
- Tests runnable via CTest
- CI builds with warnings-as-errors

## Test strategy

| Type        | Purpose                          | Location             |
|-------------|----------------------------------|----------------------|
| Unit        | Pure logic, deterministic        | `tests/unit/`        |
| Golden      | Byte-for-byte output             | `tests/golden/`      |
| Fuzz        | No crash/hang on arbitrary input | `tests/fuzz/`        |
| Integration | PTY/ConPTY real terminals        | `tests/integration/` |

## Fuzz budgets

- PR CI smoke: **5 seconds per target**
- Nightly: **60 seconds per target** with sanitizers

## Sanitizers

- Linux/macOS (Clang): ASan + UBSan in CI
- Windows: clang-cl sanitizers if feasible

## Running tests

```bash
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
ctest --test-dir out/build/posix-clang-debug --output-on-failure
```
