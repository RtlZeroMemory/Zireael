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

Notes:

- Unit tests that exercise engine wiring use an OS-header-free mock platform backend (see `tests/unit/mock_platform.c`).

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
- Smoke budgets are configurable via environment for CI/nightly scaling:
  - `ZR_FUZZ_ITERS` (default `1000`)
  - `ZR_FUZZ_MAX_SIZE` (default `512`)

Location:

- `tests/fuzz/`

## Coverage-guided fuzzing (optional)

Purpose:

- Use libFuzzer-guided corpus mutation for deeper parser exploration than fixed
  smoke seeds can provide.

Build contract:

- Enable with `-DZIREAEL_BUILD_LIBFUZZER=ON` (Clang-only).
- Harness binaries are built under `out/build/<preset>/tests/`:
  - `zireael_libfuzz_drawlist_parser`
  - `zireael_libfuzz_input_parser`
  - `zireael_libfuzz_utf8_decode`
  - `zireael_libfuzz_grapheme_iter`

## Integration tests

Purpose:

- Validate cross-module behavior that depends on platform backends (PTY/ConPTY).

Notes:

- Integration tests may be skipped in environments where required facilities are unavailable.

Location:

- `tests/integration/`
