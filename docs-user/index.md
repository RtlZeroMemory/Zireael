# Zireael

<div class="hero" markdown>

**A deterministic terminal UI core engine in C**

The foundation for building cross-platform TUI frameworks.

[Get Started](getting-started/build-and-test.md){ .md-button .md-button--primary }
[API Reference](c-api/){ .md-button }

</div>

---

## What is Zireael?

Zireael is a low-level terminal rendering engine designed to be **embedded in higher-level TUI frameworks**. It handles the hard parts—terminal I/O, input parsing, efficient diff-based rendering, Unicode grapheme handling—so framework authors don't have to.

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

---

## Features

<div class="grid cards" markdown>

-   :material-memory:{ .lg .middle } **Zero Allocations at Boundary**

    ---

    Caller provides buffers for drawlists and events. Engine never returns heap pointers that require caller `free()`.

    [:octicons-arrow-right-24: Ownership model](concepts/ownership.md)

-   :material-check-all:{ .lg .middle } **Deterministic**

    ---

    Same inputs + same config + same version pins = same outputs. Always. Reproducible behavior across runs.

    [:octicons-arrow-right-24: Determinism pins](concepts/determinism.md)

-   :material-unicode:{ .lg .middle } **Unicode 15.1**

    ---

    Full grapheme segmentation, character width calculation, and text wrapping. Handles emoji, CJK, and combining marks correctly.

    [:octicons-arrow-right-24: Unicode module](modules/unicode-text.md)

-   :material-swap-horizontal:{ .lg .middle } **Diff Renderer**

    ---

    Compares previous and current frame, emits minimal escape sequences. No full-screen redraws unless necessary.

    [:octicons-arrow-right-24: Diff renderer](modules/diff-renderer.md)

-   :fontawesome-brands-windows:{ .lg .middle } **Cross-Platform**

    ---

    Windows (ConPTY), Linux, and macOS from a single codebase. Platform code isolated behind a clean interface.

    [:octicons-arrow-right-24: Platform backends](platform/win32.md)

-   :material-file-document-check:{ .lg .middle } **Stable ABI**

    ---

    Versioned binary formats for drawlists (v1) and event batches (v1). Format changes require version bumps.

    [:octicons-arrow-right-24: ABI overview](abi/overview.md)

</div>

---

## Who Is This For?

Zireael is for **framework authors**, not application developers.

| If you're... | Zireael provides... |
|--------------|---------------------|
| Building a TUI framework in TypeScript, Rust, Go | A stable C ABI callable via FFI |
| Tired of reimplementing terminal rendering | Tested, cross-platform terminal I/O |
| Need deterministic output for testing | Reproducible rendering behavior |
| Want to focus on your framework's API | The low-level foundation |

!!! note "Not an application framework"
    Zireael doesn't provide widgets, layouts, or application structure. It provides the rendering engine that a framework would build those features on top of.

---

## Quick Start

=== "Linux / macOS"

    ```bash
    cmake --preset posix-clang-debug
    cmake --build --preset posix-clang-debug
    ctest --test-dir out/build/posix-clang-debug --output-on-failure
    ```

=== "Windows"

    ```powershell
    .\scripts\vsdev.ps1
    cmake --preset windows-clangcl-debug
    cmake --build --preset windows-clangcl-debug
    ctest --test-dir out/build/windows-clangcl-debug --output-on-failure
    ```

[:octicons-arrow-right-24: Full build guide](getting-started/build-and-test.md)

---

## Learn More

<div class="grid cards" markdown>

-   [:octicons-book-24: **Getting Started**](getting-started/build-and-test.md)

    Build, run tests, and integrate into your project.

-   [:octicons-cpu-24: **Engine Model**](concepts/engine-model.md)

    Understand the core architecture and data flow.

-   [:octicons-file-binary-24: **ABI Reference**](abi/overview.md)

    Binary formats for drawlists and events.

-   [:octicons-code-24: **C API Reference**](c-api/)

    Generated API documentation from headers.

</div>
