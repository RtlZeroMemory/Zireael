<p align="center">
  <img width="720" alt="Zireael" src="https://github.com/user-attachments/assets/000b7a71-50ca-4d9f-9ef1-fd3cde6173d1" />
</p>

<p align="center">
  <strong>A deterministic terminal UI core engine in C</strong><br>
  <em>The foundation for building cross-platform TUI frameworks</em>
</p>

<p align="center">
  <a href="https://github.com/RtlZeroMemory/Zireael/actions/workflows/ci.yml"><img src="https://github.com/RtlZeroMemory/Zireael/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/RtlZeroMemory/Zireael/releases"><img src="https://img.shields.io/github/v/release/RtlZeroMemory/Zireael" alt="Release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue" alt="License"></a>
  <a href="https://rtlzeromemory.github.io/Zireael/"><img src="https://img.shields.io/badge/docs-GitHub%20Pages-blue" alt="Docs"></a>
</p>

---

## What is Zireael?

Zireael is a **low-level terminal rendering engine** designed to be embedded in higher-level TUI frameworks. It handles the hard parts—terminal I/O, input parsing, efficient diff-based rendering, Unicode grapheme handling—so framework authors don't have to.

```
┌─────────────────────────────────────────────────────────────┐
│  Your TUI Framework (TypeScript, Rust, Go, ...)             │
├─────────────────────────────────────────────────────────────┤
│                         FFI Boundary                        │
├─────────────────────────────────────────────────────────────┤
│  Zireael Engine                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐  │
│  │ Drawlist │→ │Framebuf  │→ │ Diff     │→ │ Platform    │  │
│  │ Parser   │  │          │  │ Renderer │  │ (Win/POSIX) │  │
│  └──────────┘  └──────────┘  └──────────┘  └─────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

**You provide:** A binary drawlist describing what to render
**Engine returns:** Packed input events (keys, mouse, resize)
**Engine handles:** Terminal setup, efficient output, Unicode width calculations

## Why Zireael?

Building a TUI framework means solving the same hard problems over and over:

- **Terminal I/O** — Raw mode, signal handling, platform differences
- **Efficient rendering** — Diff algorithms, cursor optimization, minimal escape sequences
- **Unicode** — Grapheme clusters, character widths, text wrapping
- **Input parsing** — ANSI sequences, mouse protocols, bracketed paste

Zireael solves these once, correctly, with a stable ABI that any language can call via FFI.

### Design Principles

| Principle | Implementation |
|-----------|----------------|
| **Deterministic** | Same inputs + config → same outputs. Always. |
| **Zero allocations at ABI boundary** | Caller provides buffers; engine never returns heap pointers |
| **Platform isolation** | OS headers confined to `src/platform/`; core is portable C |
| **No per-frame heap churn** | Arenas and fixed buffers for hot paths |

## Quick Example

```c
#include <zr/zr_engine.h>

int main(void) {
    zr_engine_t* engine = NULL;
    zr_engine_config_t cfg = {
        .max_width = 120,
        .max_height = 40,
    };

    // Create engine (takes ownership of terminal)
    engine_create(&engine, &cfg);

    // Main loop
    uint8_t event_buf[4096];
    while (running) {
        // Poll for input events
        int n = engine_poll_events(engine, 16, event_buf, sizeof(event_buf));
        // ... process events from event_buf ...

        // Submit drawlist (your rendering commands)
        engine_submit_drawlist(engine, drawlist_bytes, drawlist_len);

        // Present (diff and flush to terminal)
        engine_present(engine);
    }

    engine_destroy(engine);
    return 0;
}
```

## Features

- **Cross-platform** — Windows (ConPTY), Linux, macOS
- **Stable ABI** — Versioned binary formats for drawlists and events
- **Unicode 15.1** — Full grapheme segmentation and width calculation
- **Diff renderer** — Minimal terminal output via smart diffing
- **Zero dependencies** — Pure C11, no external libraries

## Documentation

📖 **[Full Documentation](https://rtlzeromemory.github.io/Zireael/)** — Getting started, concepts, API reference

Quick links:
- [Build & Test](https://rtlzeromemory.github.io/Zireael/getting-started/build-and-test/)
- [Engine Model](https://rtlzeromemory.github.io/Zireael/concepts/engine-model/)
- [ABI Overview](https://rtlzeromemory.github.io/Zireael/abi/overview/)
- [C API Reference](https://rtlzeromemory.github.io/Zireael/c-api/)

## Building

**Linux / macOS:**
```bash
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
ctest --test-dir out/build/posix-clang-debug --output-on-failure
```

**Windows (clang-cl):**
```powershell
.\scripts\vsdev.ps1
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
ctest --test-dir out/build/windows-clangcl-debug --output-on-failure
```

## Who Is This For?

Zireael is for **framework authors**, not application developers. If you're:

- Building a TUI framework in TypeScript, Rust, Go, or another language
- Tired of reimplementing terminal rendering for each platform
- Need a stable C ABI you can call via FFI

Then Zireael provides the foundation so you can focus on your framework's API and features.

## Project Status

**v1.0.0** — Engine ABI stable. Drawlist v1 and Event Batch v1 formats locked.

See [CHANGELOG.md](CHANGELOG.md) for release history.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. Key rules:

- Keep OS headers out of `src/core`, `src/unicode`, `src/util`
- Validate all binary input (drawlists, events) defensively
- No per-frame heap allocations on hot paths

## License

Apache-2.0 — See [LICENSE](LICENSE)

---

<p align="center">
  <sub>Internal engine specs for contributors: <a href="docs/00_INDEX.md">docs/00_INDEX.md</a></sub>
</p>
