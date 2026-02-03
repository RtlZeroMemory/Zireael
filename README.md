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

Modern terminal apps are interactive UIs with continuous updates. This requires:

- **High performance rendering** — large surfaces, frequent updates, minimal terminal I/O
- **A portable core** — callable from any language, reusable across products

C provides a stable ABI boundary and predictable performance. Zireael handles terminal I/O, Unicode, diff rendering, and input parsing so frameworks don't have to.

## Why Zireael?

Building a TUI framework requires solving the same problems:

- Terminal I/O — raw mode, signal handling, platform differences
- Efficient rendering — diff algorithms, cursor optimization, minimal escape sequences
- Unicode — grapheme clusters, character widths, text wrapping
- Input parsing — ANSI sequences, mouse protocols, bracketed paste

Zireael solves these once behind a strict platform boundary:

- **Binary in, binary out**: wrappers send drawlist bytes; the engine returns event batches
- **Defensive validation**: drawlists/events are treated as untrusted bytes at the boundary
- **Pinned policies**: Unicode grapheme + width policy are stable (no locale surprises)
- **Bounded work**: explicit caps for drawlist sizes, counts, and output bytes per frame
- **Backend isolation**: core/unicode/util stay OS-header-free; OS code lives in `src/platform/*`

## Non-Goals

Zireael is the rendering engine, not a framework:

- **No widgets or layouts** — that's the framework's job
- **No application state** — the engine is stateless between frames
- **No high-level text APIs** — wrappers handle text measurement, shaping
- **No networking, async I/O** — out of scope

## How it works

Per-frame wrapper loop:

1. `engine_poll_events(...)` — receive input events into caller buffer
2. Build **drawlist** bytes — commands + string/blob tables
3. `engine_submit_drawlist(...)` — engine validates and executes into framebuffer
4. `engine_present(...)` — diff, emit terminal output, single flush

Wrapper handles widgets/layout/state. Engine handles terminal correctness and output.

## Design constraints

- **Stable ABI**: plain C functions, versioned binary formats (drawlist/event batch)
- **Deterministic**: pinned versions, pinned Unicode policy, no locale dependencies
- **Bounded**: explicit limits for drawlist size, command count, output bytes per frame
- **Ownership**: engine owns allocations; wrappers provide buffers, never free engine memory
- **Platform boundary**: OS code confined to `src/platform/*`; core stays OS-header-free
- **Single flush**: one write per `engine_present()`, bounded output size

## Binary formats

**Drawlist opcodes:**

| Opcode | Version | Description |
|--------|---------|-------------|
| `CLEAR` | v1 | Clear framebuffer |
| `FILL_RECT` | v1 | Fill rectangle with style |
| `DRAW_TEXT` | v1 | Draw text from string table |
| `PUSH_CLIP` / `POP_CLIP` | v1 | Clipping stack |
| `DRAW_TEXT_RUN` | v1 | Multiple styled segments |
| `SET_CURSOR` | v2 | Cursor position, shape, visibility (12B payload) |

**Event Batch v1** — engine writes packed input events (key, text, mouse, resize, paste) into caller buffer.

Small surface area by design. Throughput comes from efficient command batching, not API breadth.

## Performance Features

The engine implements several optimizations for minimal terminal I/O:

| Feature | Benefit |
|---------|---------|
| **Synchronized Output** | Wraps frames in `CSI ?2026h/l` for tear-free rendering on supported terminals |
| **Scroll Regions** | Uses `DECSTBM + SU/SD` for bulk vertical scrolling instead of redrawing |
| **Damage Rectangles** | Tracks changed regions; skips unchanged areas during diff |
| **Cursor Protocol** | Single VT sequence per cursor state change (shape/blink/visibility) |

These are enabled automatically when the terminal supports them. Capability detection happens at engine init.

### Capability Detection

The engine probes terminal capabilities at startup:

```c
typedef struct plat_caps_t {
  plat_color_mode_t color_mode;          // 16 / 256 / RGB
  uint8_t supports_mouse;
  uint8_t supports_bracketed_paste;
  uint8_t supports_focus_events;
  uint8_t supports_osc52;                // Clipboard access
  uint8_t supports_sync_update;          // CSI ?2026
  uint8_t supports_scroll_region;        // DECSTBM
  uint8_t supports_cursor_shape;         // DECSCUSR
  uint32_t sgr_attrs_supported;          // Bold, italic, etc.
} plat_caps_t;
```

Wrappers can query negotiated capabilities via `engine_get_metrics()`.

### Metrics

Every `engine_present()` updates detailed metrics:

```c
typedef struct zr_metrics_t {
  uint64_t frame_index;
  uint32_t fps;
  uint64_t bytes_emitted_total;
  uint32_t bytes_emitted_last_frame;
  uint32_t dirty_lines_last_frame;
  uint32_t damage_rects_last_frame;      // Rectangle count
  uint32_t damage_cells_last_frame;      // Cell count
  uint8_t  damage_full_frame;            // 1 if full redraw
  // ... timing, arena stats, event stats
} zr_metrics_t;
```

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

Optional **Go** proof-of-concept driving the engine through the public ABI:

- Scenario picker menu
- "LLM Agentic Coding Emulator" — thinking, tool calls, diffs
- High-stress scenarios — Matrix Rain, Neon Particle Storm
- Performance overlay — FPS, dirty lines, bytes emitted

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

Demo exercises:

- Drawlist validation and dispatch at high command counts
- Diff renderer under heavy animation
- Single-flush-per-present behavior
- Wrapper overhead (Go FFI + drawlist construction)

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

**v1.1.0** — Stable release (Engine ABI v1, Drawlist v1/v2, Event Batch v1).

New in v1.1.0:
- Drawlist v2 with `SET_CURSOR` opcode
- Synchronized output (tear-free rendering)
- Scroll region optimization (DECSTBM)
- Damage rectangle tracking
- Enhanced capability detection

See [CHANGELOG.md](CHANGELOG.md) for details.

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
