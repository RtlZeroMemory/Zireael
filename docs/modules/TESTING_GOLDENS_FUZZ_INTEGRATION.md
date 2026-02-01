# Module — Testing: Unit, Golden, Fuzz, Integration

This document defines the repository’s testing strategy and what each class of test is responsible for pinning.

## Unit tests

Purpose:

- Validate deterministic behavior and edge cases for small, focused components.
- Exercise failure modes (`ZR_ERR_*`) without depending on OS features.

Requirements:

- No locale/wall-clock dependencies.
- Explicit, deterministic inputs and assertions.

Location:

- `tests/unit/`

## Golden tests

Purpose:

- Pin byte-for-byte stable output streams (e.g., VT/ANSI diff renderer output).
- Prevent regressions in output shaping and formatting.

Rules:

- Golden expected output is stored as bytes (`expected.bin`) and compared exactly.
- Any intentional output change MUST update the corresponding fixture in the same change.

Location:

- Harness: `tests/golden/zr_golden.c`
- Fixtures: `tests/golden/fixtures/<fixture_id>/`

## Fuzz targets (smoke mode)

Purpose:

- Ensure parsers and iterators do not crash/hang and always make progress on arbitrary bytes.
- Catch OOB/UB issues under sanitizers.

Rules:

- Fuzz targets MUST be deterministic (fixed seeds when self-generating input).
- Targets must validate progress guarantees (no infinite loops).

Location:

- `tests/fuzz/`

## Integration tests

Purpose:

- Validate cross-module behavior that depends on platform backends (PTY/ConPTY).

Notes:

- Integration tests may be skipped in environments where required facilities are unavailable.

Location:

- `tests/integration/`

