# ABI policy

This document defines what “ABI stable” means in this repository and how to evolve the C ABI and binary formats safely.

## Stability contract

Zireael’s public surface consists of:

1. The **C ABI** (public headers under `include/zr/`)
2. The **drawlist** binary format (wrapper → engine)
3. The **event batch** binary format (engine → wrapper)

All three are versioned independently and negotiated at `engine_create()` via `zr_engine_config_t`.

## What counts as an ABI break?

Examples of breaking changes:

- Changing a public function signature or behavior contract.
- Changing the meaning of a field in a public struct without version gating.
- Changing packing/alignment expectations of any on-wire format.

Examples of non-breaking changes (when done correctly):

- Adding a **new** function (C ABI minor bump).
- Adding new drawlist opcode(s) to a **new drawlist format version**.
- Adding new event record types that are **skippable by size**.

## Format evolution rules

On-wire formats are:

- little-endian
- bounds-checked
- forward-compatible via `{type, size, payload}` records

Unknown record types/opcodes must be safely skippable when the size is known.

## Deprecation

Zireael does not rely on “soft” deprecation for safety-critical behavior. If a contract must change, it must be gated by a version bump.

## Wrapper compatibility checklist

When shipping a wrapper:

- Pin the requested engine ABI and format versions explicitly.
- Reject unknown versions (do not “guess” layouts).
- Treat event/drawlist bytes as untrusted; validate sizes before reading.
- Preserve little-endian encoding (even on big-endian hosts).

## Next steps

- [Versioning](versioning.md)
- [C ABI Reference](c-abi-reference.md)

