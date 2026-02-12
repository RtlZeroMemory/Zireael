# Zireael — Version Pins (Locked)

This document centralizes determinism-critical pins.

## Library version

Pinned library version (see `include/zr/zr_version.h`):

- `ZR_LIBRARY_VERSION_MAJOR = 1`
- `ZR_LIBRARY_VERSION_MINOR = 3`
- `ZR_LIBRARY_VERSION_PATCH = 1`
- Lifecycle status: alpha

## Engine ABI

Pinned engine ABI version (see `include/zr/zr_version.h`):

- `ZR_ENGINE_ABI_MAJOR = 1`
- `ZR_ENGINE_ABI_MINOR = 1`
- `ZR_ENGINE_ABI_PATCH = 0`

## Unicode

Pinned Unicode version (see `src/unicode/zr_unicode_pins.h`):

- Unicode: 15.1.0 (`ZR_UNICODE_VERSION_MAJOR=15`, `ZR_UNICODE_VERSION_MINOR=1`, `ZR_UNICODE_VERSION_PATCH=0`)
- Default emoji width policy: `ZR_WIDTH_EMOJI_WIDE`

## Binary formats

All on-buffer/on-wire formats are:

- little-endian
- versioned
- bounds-checked
- cap-limited

### Drawlist

Defined in `include/zr/zr_drawlist.h` (version pins are centralized in `include/zr/zr_version.h`):

- Supported drawlist header versions:
  - v1 (`ZR_DRAWLIST_VERSION_V1 = 1`) — baseline; must remain behavior-stable.
  - v2 (`ZR_DRAWLIST_VERSION_V2 = 2`) — adds new opcodes (e.g. cursor control) while preserving v1 layout rules.

Wrappers select the version via `zr_engine_config_t.requested_drawlist_version` at `engine_create()`.

### Packed event batches

Defined in `include/zr/zr_event.h` (version pins are centralized in `include/zr/zr_version.h`):

- Magic: `ZR_EV_MAGIC = 0x5645525A` (`'Z''R''E''V'` in little-endian u32)
- Version: `ZR_EVENT_BATCH_VERSION_V1 = 1`
