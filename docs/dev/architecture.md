# Architecture

Zireael is organized as a layered engine with strict dependency direction and a hard platform boundary.

## Layering

```text
util    -> foundational helpers, arenas, bounds-safe primitives
unicode -> UTF-8, grapheme, width/wrap policy
core    -> engine loop, drawlist/event pipeline, framebuffer, diff
platform-> OS-specific terminal backend (POSIX / Win32)
```

Dependency direction is intentionally one-way:

- `util` is lowest-level
- `unicode` depends on `util`
- `core` depends on `unicode` + `util`
- `platform` is selected behind platform abstraction

## Module Responsibilities

- `src/util/`: arenas, checked math, ring/vector utilities, logging
- `src/unicode/`: decoding, segmentation, width policy, wrapping primitives
- `src/core/`: engine lifecycle, event queue + packing, drawlist parser/executor, framebuffer/diff, metrics
- `src/platform/*`: raw mode, terminal size/caps, input read, output write, wait/wake

## Hard Boundary Rules

- no OS headers in `src/core`, `src/unicode`, `src/util`
- OS code only under `src/platform/posix` and `src/platform/win32`
- avoid platform `#ifdef` in deterministic core paths

Guardrail checks enforce this automatically.

## Runtime Data Flow

```text
input bytes (platform) -> parser -> event queue -> packed batch -> wrapper
wrapper drawlist bytes -> validate -> execute -> framebuffer -> diff -> output
```

## Memory and Ownership

- engine owns internal allocations
- caller provides data buffers for drawlist/event exchange
- no caller-free API pointers are returned

## Contract Priorities

1. correctness and bounds safety
2. deterministic behavior under caps
3. predictable ABI and wire-format evolution
4. low I/O overhead via diff + single flush

## Authoritative Specs

- [Internal docs index](../00_INDEX.md)
- [Repo layout](../REPO_LAYOUT.md)
- [Platform interface spec](../modules/PLATFORM_INTERFACE.md)
