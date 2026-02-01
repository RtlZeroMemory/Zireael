# Build & test

Zireael uses CMake presets (Ninja generator) and CTest.

## Presets

=== "POSIX (Linux/macOS)"

    ```text
    cmake --preset posix-clang-debug
    cmake --build --preset posix-clang-debug
    ctest --test-dir out/build/posix-clang-debug --output-on-failure
    ```

=== "Windows (clang-cl)"

    ```text
    # PowerShell
    .\scripts\vsdev.ps1
    cmake --preset windows-clangcl-debug
    cmake --build --preset windows-clangcl-debug
    ctest --test-dir out/build/windows-clangcl-debug --output-on-failure
    ```

## Guardrails

```text
bash scripts/guardrails.sh
```

This enforces:

- platform-boundary OS-header bans (core/unicode/util)
- libc policy restrictions (deterministic core)
