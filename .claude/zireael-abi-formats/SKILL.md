---
name: zireael-abi-formats
description: Maintain stable C ABI and versioned binary formats (drawlist + event batches).
metadata:
  short-description: ABI stability + binary formats
---

## When to use

Use this skill when changing:

- public engine ABI surface (function signatures, structs)
- drawlist binary format
- packed event batch format
- versioning/negotiation strategy

## Source of truth

- `docs/modules/CONFIG_AND_ABI_VERSIONING.md` — ABI and versioning rules
- `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md` — drawlist spec
- `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md` — event batch spec
- `docs/ERROR_CODES_CATALOG.md` — error code semantics
- `docs/VERSION_PINS.md` — pinned versions

## ABI rules (must follow)

- Return `0` on success, negative `ZR_ERR_*` codes on failure
- Engine MUST NOT return heap pointers requiring caller free
- Caller provides buffers:
  - drawlist bytes (input)
  - event batch buffer (output)
  - user-event payload (engine copies)

## Format rules (must follow)

All on-wire formats are:

- **Little-endian**
- **Versioned** (header contains version)
- **Bounds-checked** (no OOB reads, no UB)
- **Cap-limited** (hard caps via `zr_limits_t`)

**Drawlist:**

- Versioned header with magic, `total_size`, table offsets
- Commands are self-framed: `{opcode, size, flags, payload}`
- Parsing uses safe unaligned reads (`memcpy`), not casts

**Event batches:**

- Batch header with magic/version/total_size/event_count
- Per-record header with type/size/timestamp
- Unknown record types skippable by size (forward-compat)

## Versioning

Maintain separate version axes:

- Engine ABI version
- Drawlist format version
- Event batch format version

Negotiation at `engine_create(config)`.
