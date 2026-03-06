# Drawlist Format And Parser

This module documents ZRDL ingestion in the engine.

## Supported Version

`ZR_DRAWLIST_VERSION_V1` and `ZR_DRAWLIST_VERSION_V2` are accepted.

`ZR_DRAWLIST_VERSION_V1` is the baseline format. `ZR_DRAWLIST_VERSION_V2`
is additive and only gates `ZR_DL_OP_BLIT_RECT`; the other currently pinned
opcodes, including `DRAW_CANVAS` and `DRAW_IMAGE`, remain valid in v1.

## Frame Structure

- Fixed 64-byte header
- Command stream payload
- Header string/blob table fields are reserved and must be zero

See [ABI drawlist format](../abi/drawlist-format.md) for byte-level layouts.

## Parser Responsibilities

`zr_dl_validate()` performs strict structural validation before execution:

- magic/version/header checks
- alignment and bounds checks
- command stream framing and per-opcode size checks
- zero checks for reserved fields

Any invalid framing returns a validation error.

## Execution Responsibilities

`zr_dl_execute()` resolves draw command resource IDs through engine-owned
persistent stores:

- string store: `id -> bytes`
- blob store: `id -> bytes`

`DEF_*` overwrites replace previous bytes for the same ID.
`FREE_*` invalidates IDs for subsequent references.

If a draw command references an unknown ID, execution fails.

## Version-Rejection Behavior

Versions outside the current pinned set are rejected. The parser/executor share
the same core path for v1/v2 and do not retain legacy experimental negotiation
paths beyond the current public pins.
