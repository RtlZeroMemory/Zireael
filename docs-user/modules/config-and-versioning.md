# Module: Config & ABI versioning

This module covers:

- ABI version pins and negotiation at `engine_create()`
- the public config structs used at create-time and at runtime
- determinism-critical “pins” that must not vary across builds

## Source of truth

- Public headers: `include/zr/zr_config.h`, `include/zr/zr_version.h`
- Internal spec (normative): `docs/modules/CONFIG_AND_ABI_VERSIONING.md`

## Version negotiation (v1)

`engine_create()` takes `zr_engine_config_t`, which includes requested versions for:

- engine ABI (`ZR_ENGINE_ABI_*`)
- drawlist format (`ZR_DRAWLIST_VERSION_V1`)
- packed event batch format (`ZR_EVENT_BATCH_VERSION_V1`)

In v1, requested versions must match pinned versions exactly. If any requested
version is not supported, `engine_create()` fails with `ZR_ERR_UNSUPPORTED` and
performs no partial effects.

## Runtime vs create-time config

- `zr_engine_config_t`: create-time negotiation and initial configuration.
- `zr_engine_runtime_config_t`: runtime reconfiguration via `engine_set_config()`.

Both are POD, fixed-width, and have reserved/padding fields that must be zero
in v1 for ABI stability.
