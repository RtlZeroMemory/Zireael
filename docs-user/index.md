# Zireael

Zireael is a cross-platform **terminal UI core engine** written in C.

You embed it in a wrapper or application by:

- submitting a **binary drawlist** (wrapper → engine)
- polling a **packed event batch** (engine → wrapper)
- letting the engine own terminal I/O (raw mode + output emission + input bytes)

This documentation set is the user-facing entrypoint (API/ABI, integration,
and guarantees). Developer/internal engine specs live under `docs/`.

## Key properties

- Engine-owned allocations; the caller never frees engine memory.
- Caller-owned buffers for drawlists and event batches (FFI-friendly).
- Deterministic behavior for a given set of inputs + version pins + caps.
- Hard platform boundary: OS headers are isolated to `src/platform/**`.

## Start here

- [Build & test](getting-started/build-and-test.md)
- [Minimal example](getting-started/minimal-example.md)
- [ABI overview](abi/overview.md)
- [API header map](api/headers.md)
