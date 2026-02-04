# Code of Conduct

This project follows the **Contributor Covenant Code of Conduct v2.1**.

## Our Pledge

We pledge to make participation in our community a harassment-free experience for everyone, regardless of age, body size, visible or invisible disability, ethnicity, sex characteristics, gender identity and expression, level of experience, education, socio-economic status, nationality, personal appearance, race, religion, or sexual identity and orientation.

## Our Standards

Examples of behavior that contributes to a positive environment:

- Showing empathy and kindness toward other people
- Being respectful of differing opinions, viewpoints, and experiences
- Giving and gracefully accepting constructive feedback
- Accepting responsibility and apologizing to those affected by our mistakes
- Focusing on what is best for the community

Examples of unacceptable behavior:

- The use of sexualized language or imagery, and sexual attention or advances
- Trolling, insulting or derogatory comments, and personal or political attacks
- Public or private harassment
- Publishing others’ private information, such as a physical or email address, without their explicit permission
- Other conduct which could reasonably be considered inappropriate in a professional setting

## Enforcement Responsibilities

Project maintainers are responsible for clarifying and enforcing standards of acceptable behavior and will take appropriate and fair corrective action in response to any behavior they deem inappropriate.

## Scope

This Code of Conduct applies within all community spaces and also applies when an individual is officially representing the community in public spaces.

## Enforcement

To report violations, please open a GitHub issue (or, for sensitive reports, use GitHub Security Advisories if applicable). Maintainers will respond as promptly as practical.

## Attribution

This Code of Conduct is adapted from the Contributor Covenant, version 2.1.

*** Add File: GOVERNANCE.md
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

