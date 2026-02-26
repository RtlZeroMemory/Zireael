# Versioning

Zireael keeps separate version axes for API and wire formats.

## Version Axes

- **Library version** - release identifier
- **Engine ABI version** - public C ABI compatibility
- **Drawlist format version** - wrapper -> engine wire format
- **Event batch format version** - engine -> wrapper wire format

Wrappers negotiate requested versions during `engine_create()`.

## Source Of Truth

Pinned macros live in `include/zr/zr_version.h`.

Current pins:

- Library: v1.3.8
- Lifecycle: alpha
- Engine ABI: v1.2.0
- Drawlist formats: v1
- Event batch format: v1

## Compatibility Expectations

If a wrapper requests pinned versions exactly, behavior should remain consistent across builds of that same pin set.

Negotiation rules (v1 line):

- engine ABI request must match pinned ABI exactly
- event batch version must match pinned event version exactly
- drawlist request must match `ZR_DRAWLIST_VERSION_V1`

Unsupported requests fail with `ZR_ERR_UNSUPPORTED`.

## Safe Evolution Model

Non-breaking examples:

- new API function in ABI minor bump
- new drawlist opcode introduced in a new drawlist format version
- new event record type that remains skippable-by-size

Recent ABI-minor additions in `v1.2.0`:

- `engine_get_terminal_profile()` public getter
- extended `zr_terminal_caps_t` fields (`terminal_id`, cap flags/masks)
- create/runtime capability override masks (`cap_force_flags`, `cap_suppress_flags`)

Breaking examples:

- changing existing struct field meaning without version gating
- changing existing wire layout in place
- changing result semantics for existing function contract

## Wrapper Guidance

- Load pin constants from generated bindings or header mirrors.
- Reject unknown versions explicitly.
- Keep binary parsing strict (bounds/size checks on every record).

## Related Docs

- [ABI Policy](abi-policy.md)
- [C ABI Reference](c-abi-reference.md)
- [Release Model](../release-model.md)
- [Internal Config + Versioning Spec](../modules/CONFIG_AND_ABI_VERSIONING.md)
