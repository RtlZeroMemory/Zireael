# Testing

Zireael is tested via:

- **Unit tests**: deterministic core/util/unicode logic.
- **Golden tests**: byte-for-byte diff-output pinning.
- **Fuzz smoke tests**: parsers/iterators make progress and don’t crash/hang.
- **Integration tests**: PTY/ConPTY raw-mode lifecycle and wake behavior.

See the normative testing spec: `docs/modules/TESTING_GOLDENS_FUZZ_INTEGRATION.md`.

## Run tests

After configuring/building a preset:

```text
ctest --test-dir out/build/<preset> --output-on-failure
```
