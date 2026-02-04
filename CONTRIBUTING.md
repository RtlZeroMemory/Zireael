# Contributing

Zireael is a C core engine with strict determinism and safety requirements.

## Ground Rules

1. **Platform boundary**: OS headers only in `src/platform/posix/` and `src/platform/win32/`
2. **Input validation**: Treat all wrapper-provided bytes as untrusted
3. **Error contract**: `0 = OK`, negative `ZR_ERR_*` codes, no partial effects
4. **No heap churn**: Use arenas and caller-provided buffers on hot paths

## Source of Truth

Internal specs live in `docs/` (start at `docs/00_INDEX.md`). If code conflicts with internal specs, fix the code.

Wrapper-facing documentation is published via MkDocs (also sourced from `docs/`).

## Development Setup

```bash
# Clone
git clone https://github.com/RtlZeroMemory/Zireael.git
cd Zireael

# Build (Linux/macOS)
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug

# Test
ctest --test-dir out/build/posix-clang-debug --output-on-failure

# Guardrails (platform boundary + libc policy)
bash scripts/guardrails.sh
```

Windows requires running `.\scripts\vsdev.ps1` first.

## Code Style

See `docs/CODE_STANDARDS.md`. Key points:

- Every file needs a top-of-file comment explaining what/why
- Comments explain why, not what
- Extract magic numbers to named constants
- Functions: 20-40 lines, max 50
- Use `!ptr` for NULL checks

## Pull Requests

1. Fork and create a feature branch
2. Write tests for new functionality
3. Ensure all tests pass and guardrails are clean
4. Submit PR with clear description of changes

Recommended pre-flight checks:

```bash
# Format (if clang-format is installed)
bash scripts/clang_format_check.sh --all

# Docs
bash scripts/docs.sh build
```

## What We Accept

- Bug fixes with regression tests
- Performance improvements with benchmarks
- New features aligned with the roadmap
- Documentation improvements

## What We Don't Accept

- Breaking ABI changes without major version bump
- OS headers in core/unicode/util
- Per-frame heap allocations
- Features that compromise determinism
