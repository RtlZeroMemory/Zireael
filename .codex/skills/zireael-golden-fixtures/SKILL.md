---
name: zireael-golden-fixtures
description: Maintain deterministic diff-output golden tests with a single canonical fixture format.
metadata:
  short-description: Goldens + byte streams
---

## When to use

Use this skill when:

- implementing or changing diff renderer output bytes
- changing cursor movement / SGR emission rules
- changing color degradation or attribute support behavior
- adding new golden fixtures or a golden harness

## Source of truth (locked)

- `docs/GOLDEN_FIXTURE_FORMAT.md` (single source for fixture storage + comparison)
- `docs/modules/DIFF_RENDERER_AND_OUTPUT_EMITTER.md` (diff renderer behavior + core fixtures)
- `docs/VERSION_PINS.md` (pinned policies that affect output determinism)

## Golden invariants (must follow)

- Goldens compare **raw bytes** byte-for-byte (`expected.bin` vs actual).
- No normalization is allowed for diff-output goldens.
- Fixtures must pin:
  - `plat_caps_t` (color mode, attr support, unicode assumed)
  - initial terminal state (`zr_term_state_t`)
  - Unicode width policy

## Adding a new fixture (checklist)

1. Create a directory: `tests/golden/fixtures/<fixture_id>/`
2. Add:
   - `case.txt` (meta + prev/next grids)
   - `expected.bin` (canonical bytes)
   - `expected.hex.txt` (review hex dump; optional but recommended)
3. Ensure `cols/rows` and the grid widths match exactly.
4. For wide glyphs in the grid, use the continuation marker (`~`) as specified.
5. If style variation is needed, use the optional `style` grid and define `styleA.*` in `meta`.

## Review checklist (before finalizing)

- Golden fixtures are minimal (small grids) and target one behavior each.
- If output bytes changed intentionally, fixtures are updated intentionally with rationale.
- Capability-driven differences are represented by separate fixtures (e.g., truecolor vs 256 vs basic), not by normalization.

