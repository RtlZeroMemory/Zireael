# Zireael

Zireael is a deterministic terminal rendering engine in C for embedding in higher-level TUI frameworks.

It exposes a **small public C ABI** and two versioned binary protocols:

- **Drawlist** (wrapper -> engine): render commands in a little-endian byte stream
- **Event batch** (engine -> wrapper): packed input events in a little-endian byte stream

```text
+--------------------------------------------------------------+
| Wrapper / Host (Rust, Go, Python, C#, etc.)                 |
+--------------------------------------------------------------+
|                    FFI boundary (C ABI)                      |
+--------------------------------------------------------------+
| Zireael Engine                                               |
| drawlist parser -> framebuffer -> diff renderer -> platform  |
+--------------------------------------------------------------+
```

## Start Here By Goal

- Integrate quickly: [Getting Started -> Quickstart](getting-started/quickstart.md)
- Build from source: [Getting Started -> Install & Build](getting-started/install-build.md)
- Build bindings/wrappers: [ABI -> ABI Policy](abi/abi-policy.md) and [ABI -> C ABI Reference](abi/c-abi-reference.md)
- Parse binary formats directly: [ABI -> Drawlist Format](abi/drawlist-format.md) and [ABI -> Event Batch Format](abi/event-batch-format.md)
- Understand internal implementation contracts: [Internal Specs -> Index](00_INDEX.md)

## Non-Negotiable Contracts

- Stable public ABI surface under `include/zr/`
- Ownership model: engine owns engine allocations; caller never frees engine memory
- Platform boundary: OS code in `src/platform/*` only
- Deterministic limits and version pins
- Single platform flush per successful `engine_present()`

## Audience Map

- Wrapper authors: `docs/abi/*`, `docs/getting-started/*`, `docs/examples/*`
- Engine contributors: `docs/dev/*`, `docs/CODE_STANDARDS.md`, `docs/SAFETY_RULESET.md`
- Maintainers/releasers: `docs/maintainers.md`, `docs/release-model.md`, `docs/VERSION_PINS.md`, `CHANGELOG.md`

## Next Steps

- [Quickstart](getting-started/quickstart.md)
- [Concepts](user-guide/concepts.md)
- [C ABI Reference](abi/c-abi-reference.md)
