# Zireael

A deterministic terminal UI core engine in C.

Zireael handles terminal I/O, input parsing, diff-based rendering, and Unicode text handling. You embed it via FFI and drive it with binary commands.

## Quick Overview

```
┌──────────────────────────────────────────────────────────────┐
│  Your Framework (TypeScript, Rust, Go, ...)                  │
├──────────────────────────────────────────────────────────────┤
│                        FFI Boundary                          │
├──────────────────────────────────────────────────────────────┤
│  Zireael Engine                                              │
│  ┌───────────┐  ┌───────────┐  ┌────────┐  ┌──────────────┐  │
│  │ Drawlist  │→ │Framebuffer│→ │  Diff  │→ │   Platform   │  │
│  │  Parser   │  │           │  │Renderer│  │ (Win/POSIX)  │  │
│  └───────────┘  └───────────┘  └────────┘  └──────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

**You provide:** Binary drawlist (rendering commands)
**Engine returns:** Packed event batch (input events)
**Engine handles:** Terminal setup, efficient output, Unicode width

## Start Here

<div class="grid cards" markdown>

-   :material-code-tags:{ .lg .middle } **Examples**

    Complete C and Go examples showing usage patterns.

    [:octicons-arrow-right-24: View Examples](getting-started/examples.md)

-   :material-file-document:{ .lg .middle } **ABI Reference**

    Binary formats for FFI integration.

    [:octicons-arrow-right-24: ABI Overview](abi/overview.md)

-   :material-file-code:{ .lg .middle } **Drawlist v1**

    Command stream format specification.

    [:octicons-arrow-right-24: Drawlist Format](abi/drawlist-v1.md)

-   :material-message:{ .lg .middle } **Event Batch v1**

    Input event format specification.

    [:octicons-arrow-right-24: Event Format](abi/event-batch-v1.md)

</div>

## Key Properties

| Property | Description |
|----------|-------------|
| Deterministic | Same inputs + config = same outputs |
| Zero allocations at boundary | Caller provides buffers |
| Single flush per frame | One write() call per present() |
| Unicode 15.1 | Grapheme segmentation, width calculation |
| Cross-platform | Windows (ConPTY), Linux, macOS |

## Target Audience

Zireael is for **framework authors** building TUI libraries. It is not an application framework—no widgets, no layouts. It provides the rendering engine that frameworks build on.

## Version

Engine ABI v1.0.0. Drawlist v1. Event Batch v1.
