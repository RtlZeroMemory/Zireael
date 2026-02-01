# Module — Config and ABI Versioning

Zireael exposes a stable C ABI surface and versioned binary formats for wrapper consumption.

## Principles

- ABI negotiation happens at engine creation time.
- Binary formats are versioned and self-framed so unknown records can be skipped.
- The engine does not return heap pointers that require the caller to free.

See also:

- `docs/VERSION_PINS.md`
- `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md`
- `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`

