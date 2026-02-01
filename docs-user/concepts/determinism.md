# Determinism pins

Zireael is designed to be deterministic across platforms and toolchains when:

- inputs are the same (drawlists + input bytes)
- caps/config are the same
- pinned versions are the same

Pins include:

- engine ABI and binary format versions (`src/core/zr_version.h`)
- Unicode data and policies (`src/unicode/zr_unicode_pins.h`)

See `docs/VERSION_PINS.md` for the normative pin set.
