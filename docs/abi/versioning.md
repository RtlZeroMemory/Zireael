# Versioning

Zireael maintains separate version axes:

- **Engine ABI version** (C function surface and struct layouts)
- **Drawlist format version**
- **Event batch format version**

Wrappers select requested versions at `engine_create()`.

## Source of truth

Public version pins are defined in `include/zr/zr_version.h`.

Current pins:

- Library: v1.2.0
- Engine ABI: v1.1.0
- Drawlist formats: v1 and v2
- Event batch format: v1

## Compatibility expectations

- A wrapper that requests `(ABI v1.1.0, drawlist v1, event v1)` must behave identically across builds using those pins.
- New functionality is introduced via:
  - ABI minor bumps (new functions / new config fields in a version-safe way)
  - new drawlist/event versions

## Next steps

- [C ABI Reference](c-abi-reference.md)
- [Internal Specs → Config + ABI Versioning](../modules/CONFIG_AND_ABI_VERSIONING.md)
