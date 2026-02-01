---
name: zireael-golden-fixtures
description: Maintain deterministic diff-output golden tests with byte-for-byte fixture comparison.
metadata:
  short-description: Golden tests + fixtures
---

## When to use

Use this skill when:

- implementing or changing diff renderer output
- changing cursor movement or SGR emission rules
- changing color degradation or attribute support
- adding new golden fixtures

## Source of truth

- `docs/GOLDEN_FIXTURE_FORMAT.md` — fixture storage and comparison format
- `docs/modules/TESTING_GOLDENS_FUZZ_INTEGRATION.md` — test strategy
- `docs/VERSION_PINS.md` — pinned policies affecting output

## Golden invariants (must follow)

- Goldens compare **raw bytes** byte-for-byte
- No normalization allowed for diff-output goldens
- Fixtures must pin:
  - `plat_caps_t` (color mode, attr support)
  - initial terminal state (`zr_term_state_t`)
  - Unicode width policy

## Adding a new fixture

1. Create directory: `tests/golden/fixtures/<fixture_id>/`
2. Add:
   - `case.txt` (meta + prev/next grids)
   - `expected.bin` (canonical bytes)
   - `expected.hex.txt` (hex dump for review)
3. Ensure `cols/rows` match grid dimensions exactly
4. Use `~` as continuation marker for wide glyphs

## Review checklist

- [ ] Fixtures are minimal (small grids)
- [ ] Each fixture targets one behavior
- [ ] Output changes are intentional with documented rationale
- [ ] Capability differences use separate fixtures, not normalization
