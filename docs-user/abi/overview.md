# ABI / formats overview

Zireael exposes:

- a small C ABI surface (`engine_*` calls)
- two versioned binary formats:
  - drawlist v1 (wrapper → engine)
  - event batch v1 (engine → wrapper)

## Binding-critical rules

- Binary formats are little-endian.
- Event records are self-framed by size and 4-byte aligned.
- Reserved/padding fields in v1 structs must be `0`.

The normative format specs are:

- `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md`
- `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`
