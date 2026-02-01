# Zireael (C Core Engine)

Zireael is a cross-platform terminal UI core engine (Windows / Linux / macOS).

This repository contains **only the C engine**. A future TypeScript wrapper/product will live in a separate repo and will consume Zireael via a stable C ABI.

## Specs and architecture

- `MASTERDOC.MD` is the single source of truth (locked).
- Internal, implementation-ready architecture docs are generated under `docs/` (gitignored; not committed).

## Build (scaffold)

This repo uses CMake. See `CMakePresets.json` for starter Debug/Release presets.

