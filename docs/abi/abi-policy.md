# ABI Policy

This document defines what ABI stability means for Zireael and how to evolve public surfaces safely.

## Public Stability Surface

Zireael's compatibility contract includes all three:

1. public C ABI headers under `include/zr/`
2. drawlist binary format (wrapper -> engine)
3. event batch binary format (engine -> wrapper)

All three are versioned and negotiated at `engine_create()`.

## Core Stability Rules

- No silent behavior changes to existing public contracts.
- No in-place wire layout changes for existing versions.
- Reserved fields remain reserved until version-gated evolution.
- Unknown future records/opcodes must be rejectable or skippable safely.

## What Counts As A Breaking Change

Examples:

- changing existing function signature or return contract
- changing size/alignment/meaning of existing ABI struct fields
- changing record framing rules in place for current format version

## What Can Be Non-Breaking

When versioned correctly:

- adding new public functions
- appending fields to append-only snapshot structs (with prefix-copy compatibility)
- introducing new drawlist/event versions while preserving old ones

## Wire Format Evolution Rules

- little-endian only
- explicit size framing for records
- bounds-checked parsing before pointer derivation
- no partial effects on malformed input

## Deprecation Policy

Safety-critical behavior is not "soft-deprecated" in place.
If contracts must change, they change behind explicit version bumps.

## Wrapper Compatibility Checklist

- pin requested versions explicitly
- reject unsupported versions
- validate every size/offset before reading payloads
- skip unknown event types by `size` where forward-compatible
- zero reserved fields in produced binary payloads

## Release Checklist For ABI Changes

- update `include/zr/zr_version.h`
- update `docs/VERSION_PINS.md`
- update `docs/abi/versioning.md`
- update relevant format pages (`drawlist-format`, `event-batch-format`)
- update `CHANGELOG.md`
- ensure parser tests/fuzz/goldens cover new behavior

## Next Steps

- [Versioning](versioning.md)
- [C ABI Reference](c-abi-reference.md)
- [Release Model](../release-model.md)
- [Internal ABI Notes](../ABI_REFERENCE.md)
