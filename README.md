<p align="center">
  <img width="720" alt="Zireael" src="https://github.com/user-attachments/assets/000b7a71-50ca-4d9f-9ef1-fd3cde6173d1" />
</p>

<p align="center">
  <strong>A deterministic terminal UI core engine in C</strong><br>
  <em>The foundation for cross-platform TUI frameworks</em>
</p>

<p align="center">
  <a href="https://github.com/RtlZeroMemory/Zireael/actions/workflows/ci.yml"><img src="https://github.com/RtlZeroMemory/Zireael/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/RtlZeroMemory/Zireael/releases"><img src="https://img.shields.io/github/v/release/RtlZeroMemory/Zireael" alt="Release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue" alt="License"></a>
  <a href="https://rtlzeromemory.github.io/Zireael/"><img src="https://img.shields.io/badge/docs-GitHub%20Pages-blue" alt="Docs"></a>
</p>

---

## What is Zireael?

Zireael is a low-level terminal rendering engine for embedding in TUI frameworks. It provides a small, stable C ABI that lets any language drive rendering by submitting a versioned, binary **drawlist** and receiving a packed **event batch**.

This engine is the foundation for **Zireael-UI**, a TypeScript TUI framework (coming soon).

```
┌─────────────────────────────────────────────────────────────┐
│  Your TUI Framework (TypeScript, Rust, Go, ...)             │
├─────────────────────────────────────────────────────────────┤
│                         FFI Boundary                        │
├─────────────────────────────────────────────────────────────┤
│  Zireael Engine                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐  │
│  │ Drawlist │→ │Framebuf  │→ │   Diff   │→ │  Platform   │  │
│  │  Parser  │  │          │  │ Renderer │  │ (Win/POSIX) │  │
│  └──────────┘  └──────────┘  └──────────┘  └─────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

**You provide:** binary drawlist bytes (rendering commands)
**Engine returns:** packed event batch bytes (keys, mouse, resize, text)
**Engine handles:** terminal setup, diff-based rendering, efficient output, pinned Unicode policies

## Why Zireael?

Building a TUI framework requires solving the same hard problems repeatedly:

- Terminal I/O — raw mode, signal handling, platform differences
- Efficient rendering — diff algorithms, cursor optimization, minimal escape sequences
- Unicode — grapheme clusters, character widths, text wrapping
- Input parsing — ANSI sequences, mouse protocols, bracketed paste

Zireael solves these once behind a strict platform boundary and exposes a **deterministic**, bounded surface for wrappers.

## Design Principles

| Principle | Implementation |
|-----------|----------------|
| Deterministic | Same inputs + config = same outputs |
| Zero allocations at boundary | Caller provides buffers |
| Platform isolation | OS headers confined to `src/platform/` |
| Single flush | One write() per frame |

## What you get (as a framework author)

- A stable ABI (`engine_create`, `engine_poll_events`, `engine_submit_drawlist`, `engine_present`)
- A versioned, little-endian drawlist format (wrappers write bytes; engine validates defensively)
- A diff renderer that minimizes terminal traffic (cursor/SGR/output coalescing)
- Pinned Unicode grapheme + width policy for stable layout and wrapping
- A strict platform boundary so core stays OS-header-free

## Example

```c
#include <zr/zr_engine.h>

int main(void) {
    zr_engine_config_t cfg = zr_engine_config_default();
    cfg.requested_engine_abi_major = 1;
    cfg.requested_drawlist_version = 1;
    cfg.requested_event_batch_version = 1;

    zr_engine_t* engine = NULL;
    if (engine_create(&engine, &cfg) != ZR_OK) {
        return 1;
    }

    uint8_t event_buf[4096];
    int running = 1;

    while (running) {
        int n = engine_poll_events(engine, 16, event_buf, sizeof(event_buf));
        if (n > 0) {
            // Parse event batch from event_buf
        }

        // Build drawlist bytes...
        engine_submit_drawlist(engine, drawlist_bytes, drawlist_len);
        engine_present(engine);
    }

    engine_destroy(engine);
    return 0;
}
```

## Documentation

**[Full Documentation](https://rtlzeromemory.github.io/Zireael/)** — ABI reference, concepts, integration guide

- [ABI Overview](https://rtlzeromemory.github.io/Zireael/abi/overview/) — Functions and binary format rules
- [Drawlist v1](https://rtlzeromemory.github.io/Zireael/abi/drawlist-v1/) — Rendering command format
- [Event Batch v1](https://rtlzeromemory.github.io/Zireael/abi/event-batch-v1/) — Input event format
- [Configuration](https://rtlzeromemory.github.io/Zireael/abi/config/) — Config struct layout

## Building

**Linux / macOS:**
```bash
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
ctest --test-dir out/build/posix-clang-debug --output-on-failure
```

**Windows:**
```powershell
.\scripts\vsdev.ps1
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
ctest --test-dir out/build/windows-clangcl-debug --output-on-failure
```

## DEMO — Go stress-test TUI

This repo includes an optional proof-of-concept **Go** wrapper demo that drives the engine through the public ABI:

- scenario picker (menu)
- “LLM Agentic Coding Emulator” scenario (thinking, tool calls, diffs)
- high-stress scenarios (Matrix Rain + Neon Particle Storm with large per-frame command counts)
- performance overlay (FPS, Zireael dirty/bytes stats, Go RAM stats)

Run (one command; bootstraps a local Go toolchain automatically if `go` is missing):

```bash
bash scripts/poc-go-codex-tui.sh
```

Windows (PowerShell; builds `zireael.dll` and runs the demo natively):

```powershell
.\scripts\poc-go-codex-tui.ps1
```

Windows notes:

- Requires Visual Studio 2022 (or Build Tools) with C++ + Windows SDK.
- If your PowerShell execution policy blocks scripts, run: `powershell -ExecutionPolicy Bypass -File .\scripts\poc-go-codex-tui.ps1`

Build preset override (optional; defaults to release presets for smoother rendering):

```bash
ZIREAEL_PRESET=posix-clang-release bash scripts/poc-go-codex-tui.sh
```

Scenario start examples:

```bash
bash scripts/poc-go-codex-tui.sh -scenario agentic
bash scripts/poc-go-codex-tui.sh -scenario matrix
bash scripts/poc-go-codex-tui.sh -scenario storm
```

Optional frame cap (useful for stable comparisons):

```bash
bash scripts/poc-go-codex-tui.sh -scenario matrix -fps 60
```

Notes:

- `-fps` is a *cap*. Omit it (default) to run uncapped.
- Many terminal emulators effectively refresh around ~60–144 Hz; uncapped mode may still report FPS near that while you increase backend stress via `-storm-n` and `-phantom`.

Benchmark example (Neon Particle Storm):

```bash
bash scripts/poc-go-codex-tui.sh -scenario storm -bench-seconds 10 -storm-n 150000 -storm-visible 25000 -phantom 200000
```

What the demo stresses:

- drawlist validation + dispatch at high command counts
- diff renderer behavior under heavy animation
- platform flush behavior (single flush per present)
- wrapper overhead (Go drawlist building + string/blob management)

Example benchmark output (POSIX release preset):

```
Go PoC summary:
  duration: 3s
  frames:   448
  avg_fps:  149.2
  bytes_total: 10092464
  bytes_last:  24684
```

Source: `poc/go-codex-tui/`

## Target Audience

Zireael is for **framework authors**, not application developers. Use it if you're building a TUI framework and need a stable C ABI for terminal rendering.

## Status

**v1.0.0** — Engine ABI stable. Drawlist v1 and Event Batch v1 locked.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Key rules:

- OS headers stay in `src/platform/`
- Validate all binary input defensively
- No heap allocations on hot paths

## License

Apache-2.0

---

<p align="center">
  <sub>Internal specs for contributors: <a href="docs/00_INDEX.md">docs/00_INDEX.md</a></sub>
</p>
