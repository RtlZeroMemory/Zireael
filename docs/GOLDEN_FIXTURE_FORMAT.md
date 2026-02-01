# Zireael — Golden Fixture Format (Locked)

Golden tests compare engine output bytes byte-for-byte for determinism.

## Directory layout

Golden fixtures live under:

- `tests/golden/fixtures/<fixture_id>/expected.bin` (required)
- `tests/golden/fixtures/<fixture_id>/case.txt` (required; human-readable context/pins)
- `tests/golden/fixtures/<fixture_id>/expected.hex.txt` (optional; hex bytes for review)

## Comparison rule

- Tests load `expected.bin` and compare exact length and byte content.
- On mismatch, the harness prints the first mismatch offset and hex context.

See implementation:

- `tests/golden/zr_golden.c`

