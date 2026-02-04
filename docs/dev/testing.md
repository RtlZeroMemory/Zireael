# Testing

Zireael uses multiple tiers of tests:

- Unit tests (`tests/unit/`)
- Golden tests (`tests/golden/`)
- Fuzz targets (`tests/fuzz/`)
- Integration tests (`tests/integration/`)

## Run

```bash
ctest --test-dir out/build/posix-clang-debug --output-on-failure
```

## Sanitizers

On Linux, CI runs ASan+UBSan builds via a dedicated preset.

## Next steps

- [Internal Specs → Testing](../modules/TESTING_GOLDENS_FUZZ_INTEGRATION.md)
- [Debugging](debugging.md)

