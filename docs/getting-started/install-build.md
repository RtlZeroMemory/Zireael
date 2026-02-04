# Install & build

Zireael is a C library built with **CMake presets** and tested with **CTest**.

## Prerequisites

Minimums:

- CMake 3.21+
- Ninja (recommended; presets assume Ninja)
- A C11 compiler

## Presets

The canonical entrypoints are `CMakePresets.json` and `ctest`.

### POSIX (Linux/macOS)

```bash
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
ctest --test-dir out/build/posix-clang-debug --output-on-failure
```

### Windows (clang-cl)

```powershell
.\scripts\vsdev.ps1
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
ctest --test-dir out/build/windows-clangcl-debug --output-on-failure
```

## Guardrails

Run:

```bash
bash scripts/guardrails.sh
```

This enforces (among other things):

- platform-boundary OS-header bans (core/unicode/util)
- libc policy restrictions (deterministic core)

## Next steps

- [FAQ](faq.md)
- [Dev → Build System](../dev/build-system.md)
- [Dev → Testing](../dev/testing.md)

