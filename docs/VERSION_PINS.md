# Zireael — Version Pins (Locked)

This document centralizes determinism-critical pins.

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

- Drawlist header `version` is pinned to v1 (`1`).

### Packed event batches

Defined in `src/core/zr_event.h` (version pins are centralized in `src/core/zr_version.h`):

- Magic: `ZR_EV_MAGIC = 0x5645525A` (`'Z''R''E''V'` in little-endian u32)
- Version: `ZR_EVENT_BATCH_VERSION_V1 = 1`
