<p align="center">
  <img width="720" alt="Zireael" src="https://github.com/user-attachments/assets/179a0cbe-b3f1-410c-a99a-537781a1134d" />
</p>

<p align="center">
  <em>The foundation for cross-platform TUI frameworks</em>
</p>

<p align="center">
  <a href="https://github.com/RtlZeroMemory/Zireael/actions/workflows/ci.yml"><img src="https://github.com/RtlZeroMemory/Zireael/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/RtlZeroMemory/Zireael/releases"><img src="https://img.shields.io/github/v/release/RtlZeroMemory/Zireael" alt="Release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue" alt="License"></a>
  <a href="https://rtlzeromemory.github.io/Zireael/"><img src="https://img.shields.io/badge/docs-GitHub%20Pages-blue" alt="Docs"></a>
</p>

---

![Zireael-Matrix-1](https://github.com/user-attachments/assets/463365ae-55cc-41af-b562-b20473c6452e)



## What is Zireael?

Zireael is a low-level terminal rendering engine for embedding in higher-level TUI frameworks. It provides a small, stable C ABI that lets any language drive rendering by submitting a versioned, binary **drawlist** and receiving a packed **event batch**.

This engine is being built to power **Zireael-UI** (a TypeScript framework in progress), and to serve as a reusable, performance-focused core for other wrappers (Rust, Go, etc.).

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
**Engine handles:** terminal init/restore (raw mode, alt-screen, input modes), diff-based rendering + output coalescing, pinned Unicode policies

## Motivation

Modern terminal apps are no longer just “CLI output”. They are interactive UIs, often running long sessions, and increasingly used for workflows like agentic coding where the UI updates continuously while the program performs real work in the background.

In that environment you want two things at the same time:

- **High performance rendering** (large surfaces, frequent updates, minimal terminal I/O).
- **A portable core** that can be called from any language and reused across products.

C is a practical choice for the core: it gives a stable ABI boundary and predictable performance characteristics. Zireael exists so higher-level frameworks don’t have to reimplement terminal I/O, Unicode policies, diff rendering, input parsing, and the “edge case” behavior that makes terminal UIs reliable across platforms and terminals.

## Why Zireael?

Building a TUI framework requires solving the same hard problems repeatedly:

- Terminal I/O — raw mode, signal handling, platform differences
- Efficient rendering — diff algorithms, cursor optimization, minimal escape sequences
- Unicode — grapheme clusters, character widths, text wrapping
- Input parsing — ANSI sequences, mouse protocols, bracketed paste

Zireael solves these once behind a strict platform boundary and exposes a deterministic, bounded surface for wrappers:

- **Binary in, binary out**: wrappers send drawlist bytes; the engine returns event batches.
- **Defensive validation**: drawlists/events are treated as untrusted bytes at the boundary.
- **Pinned policies**: Unicode grapheme + width policy are stable and deterministic (no locale-dependent surprises).
- **Bounded work**: explicit caps for drawlist sizes, counts, and output bytes per frame.
- **Backend isolation**: core/unicode/util stay OS-header-free; OS code lives in `src/platform/*`.

## How it works 

Per frame, a wrapper typically does:

1. `engine_poll_events(...)` into a caller-provided byte buffer (keys, mouse, resize, text).
2. Build a **drawlist v1** byte stream (little-endian): a command section plus string/blob tables.
3. `engine_submit_drawlist(engine, bytes, len)` (engine validates limits and format).
4. Engine executes commands into an internal framebuffer, diffs against the previous frame, and builds terminal output into an internal bounded buffer.
5. `engine_present(engine)` flushes once for that frame and updates metrics.

This keeps the wrapper focused on widgets/layout/state while the engine owns terminal correctness, Unicode policies, and output performance.

## Design constraints 

- **Stable ABI + formats**: plain C functions and versioned on-wire formats (drawlist/event batch) for wrappers in any language.
- **Deterministic behavior**: pinned ABI/format versions and pinned Unicode width/grapheme policy (no locale-dependent width tables).
- **Bounded work**: limits for drawlist bytes/cmds/strings/blobs and output bytes per frame; validation rejects invalid/oversized inputs.
- **Ownership model**: engine owns its allocations; wrappers provide event buffers and never free engine memory.
- **Platform boundary**: OS code is confined to `src/platform/*`; core/unicode/util do not include OS headers.
- **Output policy**: diff renderer coalesces terminal operations and flushes once per `engine_present()` (bounded by `out_max_bytes_per_frame`).

## What a wrapper actually sends

Drawlist v1 supports a small set of opcodes:

- `CLEAR`
- `FILL_RECT`
- `DRAW_TEXT` (string-table slices)
- `PUSH_CLIP` / `POP_CLIP`
- `DRAW_TEXT_RUN` (multiple styled segments in one command)

Event Batch v1 is the inverse direction: the engine writes a packed byte stream of input events (key/text/mouse/resize) into a caller-provided buffer.

The surface area is intentionally small: throughput comes from pushing many commands/segments efficiently under configured caps, not from a large API.

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

![Zireael-Matrix-2](https://github.com/user-attachments/assets/a5f874bc-58c9-4e4d-9fab-57739a905607)
![Zireael-Matrix-3](https://github.com/user-attachments/assets/d1959f52-86c2-4668-84bb-5045b70a57a1)

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

**v1.0.0-rc1** — Public preview (Engine ABI v1, Drawlist v1, Event Batch v1).

Stability note: the ABI and formats are versioned and defensive, but we are not claiming “battle-tested stability” yet. Expect iteration as Zireael-UI and other wrappers shake out real-world edge cases.

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
