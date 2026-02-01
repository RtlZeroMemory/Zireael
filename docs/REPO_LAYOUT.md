# Zireael — Repo Layout (Normative)

Zireael is an engine-only C repository.

## Directories

- `src/util/`: foundational utilities (checked math, arenas, containers, logging).
- `src/unicode/`: UTF-8 decode, grapheme segmentation, width, wrapping.
- `src/core/`: framebuffer, drawlist parsing/execution, diff renderer, event batch ABI.
- `src/platform/`: platform boundary and OS-specific backends:
    - `src/platform/posix/`
    - `src/platform/win32/`
- `tests/`: unit, golden, fuzz, and integration tests.
- `examples/`: small C examples only.

## Dependency direction

- `util → unicode → core → platform interface → platform backends`

## Platform boundary (hard rule)

- `src/core`, `src/unicode`, and `src/util` MUST NOT include OS headers.
- OS headers and OS behavior must be isolated to `src/platform/posix` and `src/platform/win32`.

