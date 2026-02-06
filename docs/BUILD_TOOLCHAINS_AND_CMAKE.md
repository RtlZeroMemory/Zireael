# Zireael — Build Toolchains and CMake (Normative)

Zireael uses CMake presets (`CMakePresets.json`) and CTest.

## Configure / build

Typical POSIX debug build:

```text
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
ctest --test-dir out/build/posix-clang-debug --output-on-failure
```

Optional POSIX ThreadSanitizer build:

```text
cmake --preset posix-clang-tsan
cmake --build --preset posix-clang-tsan
ctest --test-dir out/build/posix-clang-tsan --output-on-failure
```

Optional POSIX libFuzzer harness build:

```text
cmake --preset posix-clang-fuzz
cmake --build --preset posix-clang-fuzz
```

Typical Windows debug build (clang-cl):

```text
# PowerShell
.\scripts\vsdev.ps1
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
ctest --test-dir out/build/windows-clangcl-debug --output-on-failure
```

## Guardrails

Run:

```text
bash scripts/guardrails.sh
```

This enforces:

- platform-boundary OS-header bans (core/unicode/util)
- forbidden libc calls in deterministic core (core/unicode/util)

## Options (high level)

Project options are defined in `CMakeLists.txt` (top-level). CI typically sets:

- `-DZIREAEL_WARNINGS_AS_ERRORS=ON`
- `-DZIREAEL_BUILD_EXAMPLES=OFF`

Optional fuzz harnesses:

- `-DZIREAEL_BUILD_LIBFUZZER=ON` builds Clang-only libFuzzer targets under
  `tests/` for coverage-guided parser fuzzing.
