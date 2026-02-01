# Zireael — Build Toolchains and CMake (Normative)

Zireael uses CMake presets (`CMakePresets.json`) and CTest.

## Configure / build

Typical POSIX debug build:

```text
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
ctest --test-dir out/build/posix-clang-debug --output-on-failure
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

See `README.md` for current option names and defaults.

