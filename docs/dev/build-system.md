# Build System

Zireael uses CMake presets as the canonical build entrypoint.

## Presets

Defined in `CMakePresets.json`:

- `posix-clang-debug`
- `posix-clang-release`
- `posix-clang-asan-ubsan`
- `posix-gcc-debug`
- `posix-gcc-release`
- `windows-clangcl-debug`
- `windows-clangcl-release`

Build output root: `out/build/<preset>`.

## Primary Targets

Always:

- `zireael` (static library)

Optional:

- `zireael_shared` (shared library, `ZIREAEL_BUILD_SHARED=ON`)
- examples (`ZIREAEL_BUILD_EXAMPLES=ON`)
- tests (`ZIREAEL_BUILD_TESTS=ON`)

## Common Configuration Flags

- `ZIREAEL_BUILD_SHARED`
- `ZIREAEL_BUILD_EXAMPLES`
- `ZIREAEL_BUILD_TESTS`
- `ZIREAEL_WARNINGS_AS_ERRORS`
- `ZIREAEL_SANITIZE_ADDRESS`
- `ZIREAEL_SANITIZE_UNDEFINED`

## Standard Workflows

### Fast local debug (POSIX)

```bash
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
ctest --preset posix-clang-debug --output-on-failure
```

### Sanitizer pass (POSIX)

```bash
cmake --preset posix-clang-asan-ubsan
cmake --build --preset posix-clang-asan-ubsan
ctest --preset posix-clang-asan-ubsan --output-on-failure
```

### Windows debug

```powershell
.\scripts\vsdev.ps1
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
ctest --preset windows-clangcl-debug --output-on-failure
```

## Notes

- Sanitizer flags are not supported for MSVC/clang-cl path in current CMake logic.
- Presets already configure `CMAKE_EXPORT_COMPILE_COMMANDS=ON`.

## Related Docs

- [Install & Build](../getting-started/install-build.md)
- [Testing](testing.md)
- [Build toolchains + CMake internal spec](../BUILD_TOOLCHAINS_AND_CMAKE.md)
