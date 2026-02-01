---
name: zireael-abi-formats
description: Maintain stable C ABI and versioned binary formats (drawlist + event batches) for wrapper consumption; no TS code.
metadata:
  short-description: ABI stability + binary formats
---

## When to use

Use this skill when changing:

- public engine ABI surface (function signatures, structs)
- drawlist binary format
- packed event batch format
- versioning/negotiation strategy
- any docs about the future TS wrapper (without adding TS code)

## ABI rules (must follow)

- Minimal ABI function surface is locked by `MASTERDOC.MD`.
- Return `0` on success and negative error codes on failure.
- Engine must not return heap pointers requiring caller free.
- Caller provides buffers:
  - drawlist bytes input
  - event batch output buffer
  - user-event payload input (engine copies)

## Format rules (must follow)

- All on-wire/on-buffer formats are:
  - **little-endian**
  - **versioned** (header contains version)
  - **bounds-checked** (no OOB reads; no UB)
  - **cap-limited** (hard caps for safety)

Drawlist:

- versioned header, explicit `total_size`, table offsets/sizes
- commands are self-framed records with `{opcode,size}` so parsing is deterministic
- parsing uses safe unaligned reads (`memcpy`), not casts

Event batches:

- batch header with magic/version/total_size/event_count
- per-record header with type/size/timestamp
- unknown record types must be skippable by size (forward-compat)

## Versioning guidance

Maintain separate version axes:

- engine ABI version (function + struct layouts)
- drawlist format version
- event batch format version

Negotiation should occur at `engine_create(config)` (requested versions + feature flags); reject incompatible combos deterministically.

## Wrapper boundary guidance (no TS code)

- Treat engine as black box via stable C ABI.
- Packaging options to keep in mind:
  - shared library + FFI
  - node-addon (N-API) linking against engine
  - static library embedded into wrapper build

This repo should contain **documentation only** about wrapper consumption (no TS, no Node tooling).

