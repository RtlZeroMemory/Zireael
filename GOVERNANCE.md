# Governance

Zireael is maintained as a low-level C engine with a strict determinism and ABI stability contract.

## Roles

- **Maintainers**: review/merge PRs, cut releases, and enforce the safety/ABI constraints.
- **Contributors**: submit PRs/issues; help with docs/tests/backends.

## Decision making

Default approach:

1. Discuss in an issue or PR thread with a concrete proposal.
2. Prefer small, reviewable changes over large refactors.
3. If consensus isn’t reached, maintainers decide with the project constraints as the primary rubric.

## PR requirements

Maintainers will not merge changes that violate:

- the platform boundary (`src/core`, `src/unicode`, `src/util` stay OS-header-free)
- the ownership model (callers never free engine memory)
- the error contract (`0 = OK`, negative `ZR_ERR_*` failures)
- the pinned determinism policies (Unicode pins, libc policy)

## Security issues

Please follow `SECURITY.md`. For memory safety or parsing issues, include a minimal reproducer and the exact versions involved.

