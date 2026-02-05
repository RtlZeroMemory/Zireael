# Style Guide

The authoritative style sources are:

- `docs/CODE_STANDARDS.md`
- `docs/SAFETY_RULESET.md`
- `docs/LIBC_POLICY.md`

This page is a practical checklist for day-to-day edits.

## Code Structure Rules

- each `.c`/`.h` file starts with a top-of-file "what/why" comment
- functions should stay focused and usually under 50 lines
- avoid magic numbers; extract named constants
- prefer explicit section markers in long functions

## Safety and Contract Rules

- treat external bytes as untrusted
- validate bounds before pointer derivation
- keep `0 = OK`, negative errors contract consistent
- avoid partial effects on validation failure paths

## Platform Boundary Rules

- no OS headers in `src/core`, `src/unicode`, `src/util`
- OS behavior belongs in `src/platform/posix` and `src/platform/win32`

## Documentation Rules

- update docs when ABI/format behavior changes
- keep version pins synchronized across headers/docs/changelog
- ensure wrapper-facing docs and internal specs do not drift

## Review Checklist

- build + relevant tests pass
- guardrails pass
- docs and version pins updated when needed
- comments explain intent/constraints, not obvious operations

## Related Docs

- [Code standards](../CODE_STANDARDS.md)
- [Header layering](../HEADER_LAYERING.md)
- [Maintainers docs checklist](../maintainers.md)
