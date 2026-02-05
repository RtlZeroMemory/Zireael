# Install & Build

Zireael is a C project built with CMake presets and tested through CTest.

## Prerequisites

Minimum:

- CMake `3.21+`
- Ninja (recommended; presets assume Ninja)
- C11 compiler toolchain

Docs tooling (optional):

- Python `3.10+`
- Doxygen (optional, for generated API HTML)

## Canonical Entry Points

- Build presets: `CMakePresets.json`
- Tests: `ctest --output-on-failure`
- Guardrails: `bash scripts/guardrails.sh`
- Docs build: `bash scripts/docs.sh build`

## Configure, Build, Test

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

## Optional Build Flags

Common CMake options (toggle per preset/toolchain flow):

- `-DZIREAEL_BUILD_TESTS=ON|OFF`
- `-DZIREAEL_BUILD_EXAMPLES=ON|OFF`
- `-DZIREAEL_BUILD_SHARED=ON|OFF`

See `CMakeLists.txt` and `docs/dev/build-system.md` for the full target layout.

## Guardrails (Required For CI Quality)

```bash
bash scripts/guardrails.sh
```

Guardrails currently enforce:

- no OS headers in `src/core`, `src/unicode`, `src/util`
- forbidden libc usage in deterministic core modules

## Docs Build

```bash
bash scripts/docs.sh build
```

This runs strict MkDocs and, when available, Doxygen generation.

## Next Steps

- [FAQ](faq.md)
- [Dev -> Build System](../dev/build-system.md)
- [Dev -> Testing](../dev/testing.md)
