# Zireael — C Core Terminal Engine

<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/000b7a71-50ca-4d9f-9ef1-fd3cde6173d1" />


Zireael is a cross-platform **terminal UI core engine** for Windows / Linux / macOS.

It is designed to be embedded: you drive it by submitting a **binary drawlist** and you read back a **packed event
batch**. The engine owns terminal I/O (raw mode, output emission, input bytes) and provides a deterministic, cap-bounded core
suitable for wrappers in other languages.

## Documentation

- User docs (in repo): `docs-user/index.md`
- User docs (GitHub Pages): https://rtlzeromemory.github.io/Zireael/
- Internal engine specs (normative): `docs/00_INDEX.md` (developer/engine implementation)

## Overview

At a glance:

- **Engine-only C library**: no TypeScript, no Node tooling, no GUI toolkit.
- **Stable ABI**: callers interact via a small `engine_*` API surface.
- **Explicit ownership**: caller provides input/output buffers; the engine never returns heap pointers that require
  caller `free()`.
- **Deterministic core**: fixed inputs + fixed caps/config ⇒ fixed outputs.
- **Hard platform boundary**: OS headers live only under `src/platform/**`.

## Quickstart

POSIX (Linux/macOS):

```text
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
ctest --test-dir out/build/posix-clang-debug --output-on-failure
```

Windows (clang-cl):

```text
# PowerShell
.\scripts\vsdev.ps1
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
ctest --test-dir out/build/windows-clangcl-debug --output-on-failure
```

## API / ABI

Public headers are under `include/zr/` (start at `include/zr/zr_engine.h`).

Binary formats and determinism are documented in:

- User docs: `docs-user/index.md`
- Internal format specs (normative): `docs/modules/`

## Contributing guidelines (high-level)

- Keep OS headers out of `src/core`, `src/unicode`, `src/util`.
- Treat drawlist/event bytes as untrusted input: validate bounds and overflow before use.
- Prefer fixed-size, caller-provided buffers on the ABI boundary.
- Avoid per-frame heap churn; use arenas and bounded containers.

See `CONTRIBUTING.md` for details.

## Security

See `SECURITY.md`.

## Changelog

See `CHANGELOG.md`.

## License

Apache-2.0 (see `LICENSE`).
