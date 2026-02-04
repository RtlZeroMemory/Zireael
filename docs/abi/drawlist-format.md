# Drawlist format

Wrappers submit rendering commands as a versioned, little-endian **drawlist** byte stream.

This page is a wrapper-facing guide. The implementation-ready spec is:

- [Internal Specs → Drawlist Format and Parser](../modules/DRAWLIST_FORMAT_AND_PARSER.md)

## Quick rules

- Little-endian.
- Bounds-checked; malformed inputs return `ZR_ERR_FORMAT` / `ZR_ERR_LIMIT`.
- Reserved fields must be zero.
- Records are self-framed (size included) so newer opcodes can be skipped safely.

## Negotiation

Wrappers select the drawlist format via `zr_engine_config_t.requested_drawlist_version` at `engine_create()`.

## Next steps

- [Event Batch Format](event-batch-format.md)
- [Internal Specs → Drawlist](../modules/DRAWLIST_FORMAT_AND_PARSER.md)

