# Drawlist Format And Parser

This module documents ZRDL ingestion in the engine.

## Supported Version

Only `ZR_DRAWLIST_VERSION_V1` is accepted.

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

## No Backward Compatibility

Legacy multi-version paths were removed. The parser/executor are single-version
(v1) by design.
