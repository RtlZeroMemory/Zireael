# Module: Unicode text

Zireael includes an OS/locale-independent Unicode subsystem for:

- UTF-8 decode with deterministic replacement behavior
- grapheme segmentation
- width policy and measurement
- wrapping

## Source of truth

- Implementation: `src/unicode/`
- Pinned versions/policies: `src/unicode/zr_unicode_pins.h`
- Internal spec (normative): `docs/modules/UNICODE_TEXT.md`

These pins exist so wrappers get stable layout across platforms/toolchains.
