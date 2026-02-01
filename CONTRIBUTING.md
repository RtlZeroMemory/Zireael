# Contributing

Zireael is a C core engine with a strict determinism and safety posture.

## Ground rules

- Platform boundary is hard: OS headers live only under `src/platform/posix/` and `src/platform/win32/`.
- Treat all wrapper-provided bytes as untrusted input (drawlists, input bytes).
- Preserve the error contract: `0 = OK`, negative `ZR_ERR_*` failures, and “no partial effects” where specified.
- Avoid per-frame heap churn; prefer arenas and caller-provided buffers.

## Source of truth

The normative internal specs live under `docs/` (start at `docs/00_INDEX.md`).
If code conflicts with `docs/`, fix the code.

## Build and test

```text
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
ctest --test-dir out/build/posix-clang-debug --output-on-failure
```

Guardrails (platform boundary + libc policy):

```text
bash scripts/guardrails.sh
```

## Code style

See `docs/CODE_STANDARDS.md`.
