# Zireael

A deterministic terminal rendering engine in C for embedding in higher-level TUI frameworks.

Zireael exposes a **small, stable C ABI**. Wrappers submit a versioned, binary **drawlist** (render commands) and receive a packed **event batch** (input events) — **binary in / binary out**.

```text
┌──────────────────────────────────────────────────────────────┐
│  Your wrapper (Rust, Go, Python, …)                           │
├──────────────────────────────────────────────────────────────┤
│                        FFI Boundary                           │
├──────────────────────────────────────────────────────────────┤
│  Zireael Engine                                               │
│  ┌───────────┐  ┌────────────┐  ┌────────┐  ┌──────────────┐ │
│  │ Drawlist  │→ │ Framebuffer│→ │  Diff  │→ │  Platform     │ │
│  │  Parser   │  │            │  │Renderer│  │ (Win/POSIX)   │ │
│  └───────────┘  └────────────┘  └────────┘  └──────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

## Start here

- If you want to integrate quickly: **[Getting Started → Quickstart](getting-started/quickstart.md)**
- If you are writing FFI bindings: **[ABI → ABI Policy](abi/abi-policy.md)** and **[ABI → C ABI Reference](abi/c-abi-reference.md)**
- If you want the exact on-wire formats: **[ABI → Drawlist Format](abi/drawlist-format.md)** and **[ABI → Event Batch Format](abi/event-batch-format.md)**

## Design constraints (non-negotiable)

- **Stable ABI:** plain C entrypoints; versioned binary formats (drawlist/event batch).
- **Ownership:** the engine owns its allocations; callers never free engine memory; callers provide I/O buffers.
- **Determinism:** pinned versions, pinned Unicode policy; no locale dependencies.
- **Platform boundary:** OS code lives in `src/platform/*`; core/unicode/util remain OS-header-free.
- **Single flush:** one terminal write per `engine_present()`, bounded by caps.

## Next steps

- [Getting Started → Install & Build](getting-started/install-build.md)
- [User Guide → Concepts](user-guide/concepts.md)
- [Examples → Overview](examples/index.md)

