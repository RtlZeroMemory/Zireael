# Testing

Zireael uses layered tests for correctness, regressions, safety, and deterministic behavior.

## Test Tiers

- Unit tests: parser/core utility behavior
- Golden tests: deterministic renderer output fixtures
- Integration tests: backend/pty/conpty behavior (platform-specific)
- Fuzz smoke tests: malformed input resilience for parsers/unicode paths

## CTest Namespaces

Current suite includes namespaces like:

- `zireael.unit`
- `zireael.golden`
- `zireael.integration.*`
- `zireael.fuzz.*`

## Run Tests

### Full preset run

```bash
ctest --preset posix-clang-debug --output-on-failure
```

### By build directory

```bash
ctest --test-dir out/build/posix-clang-debug --output-on-failure
```

### Filtered runs

```bash
ctest --test-dir out/build/posix-clang-debug -R zireael.golden --output-on-failure
ctest --test-dir out/build/posix-clang-debug -R zireael.fuzz --output-on-failure
```

## Guardrails and Drift Checks

Run alongside tests:

```bash
bash scripts/guardrails.sh
python3 scripts/check_version_pins.py
```

## CI-Oriented Recommendations

- run at least one clang and one gcc preset on POSIX
- include sanitizer preset on Linux
- run docs strict build in CI (`bash scripts/docs.sh build`)
- `scripts/docs.sh` reuses `.venv-docs` and respects `$PYTHON`, so CI can pin
  a deterministic interpreter
- treat guardrail failures as blocking

## Related Specs

- [Internal testing module spec](../modules/TESTING_GOLDENS_FUZZ_INTEGRATION.md)
- [Golden fixture format](../GOLDEN_FIXTURE_FORMAT.md)
