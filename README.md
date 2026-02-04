<p align="center">
  <img width="720" alt="Zireael" src="https://github.com/user-attachments/assets/179a0cbe-b3f1-410c-a99a-537781a1134d" />
</p>

<p align="center">
  <em>A deterministic terminal rendering engine in C</em>
</p>

<p align="center">
  <a href="https://github.com/RtlZeroMemory/Zireael/actions/workflows/ci.yml"><img src="https://github.com/RtlZeroMemory/Zireael/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/RtlZeroMemory/Zireael/releases"><img src="https://img.shields.io/github/v/release/RtlZeroMemory/Zireael" alt="Release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue" alt="License"></a>
  <a href="https://rtlzeromemory.github.io/Zireael/"><img src="https://img.shields.io/badge/docs-GitHub%20Pages-blue" alt="Docs"></a>
</p>

---

## What it is

Zireael is a **low-level terminal rendering engine** for embedding in higher-level TUI frameworks. It provides a small, stable C ABI so wrappers in any language can drive rendering by submitting a versioned **drawlist** (binary command stream) and receiving a packed **event batch** (binary input events).

```text
Wrapper                            Engine
------                            ------

drawlist bytes  ───────────────▶  engine_submit_drawlist()
                                  engine_present()   (diff + single flush)

event bytes    ◀───────────────  engine_poll_events()
```

## What it isn’t

- Not a widget/layout framework
- Not an application runtime
- Not a high-level text API

## Design constraints

- **Binary in / binary out**: drawlists and event batches are versioned formats.
- **Ownership (locked)**: the engine owns its allocations; callers never free engine memory; callers provide I/O buffers.
- **Determinism**: pinned Unicode and version pins; no locale dependencies.
- **Platform boundary**: OS code lives in `src/platform/*`; core/unicode/util remain OS-header-free.
- **Single flush per present**: one terminal write per `engine_present()`.

## Quickstart

Build (Linux/macOS):

```bash
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
./out/build/posix-clang-debug/zr_example_minimal_render_loop
```

Build (Windows, clang-cl):

```powershell
.\scripts\vsdev.ps1
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
.\out\build\windows-clangcl-debug\zr_example_minimal_render_loop.exe
```

## Documentation

- Docs site: https://rtlzeromemory.github.io/Zireael/
- Getting started: https://rtlzeromemory.github.io/Zireael/getting-started/quickstart/
- ABI policy: https://rtlzeromemory.github.io/Zireael/abi/abi-policy/
- C ABI reference: https://rtlzeromemory.github.io/Zireael/abi/c-abi-reference/

## Status

- Latest stable release: `v1.1.0`
- Latest pre-release: `v1.2.0-rc5` (see `CHANGELOG.md`)

## Contributing

See `CONTRIBUTING.md`. Internal implementation specs live in `docs/00_INDEX.md`.

## License

Apache-2.0 (see `LICENSE`).
