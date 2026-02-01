---
name: zireael-header-layering
description: Prevent include cycles and platform-boundary leaks by enforcing header responsibilities and include layering.
metadata:
  short-description: Headers + includes
---

## When to use

Use this skill when:

- adding new headers or refactoring includes
- encountering circular dependencies between core/unicode/util headers
- tightening the platform boundary (no OS headers in core/unicode/util)

## Source of truth (locked)

- `docs/HEADER_SKELETON_PLAN.md` (planned headers and what each declares)
- `docs/REPO_LAYOUT.md` (dependency direction and `#ifdef` policy)
- `docs/LIBC_POLICY.md` (stdlib usage constraints)

## Include layering rules (must follow)

- `src/util/**` must be OS-header-free.
- `src/unicode/**` may include `src/util/**` only; must be OS-header-free.
- `src/core/**` may include `src/util/**`, `src/unicode/**`, and `src/platform/zr_platform.h`; must be OS-header-free.
- `src/platform/**` may include OS headers; implements `src/platform/zr_platform.h`.

## Checklist (before finalizing)

- Prefer forward declarations in headers to avoid deep include trees.
- Keep ABI-visible headers POD-only (fixed-width ints; no pointers requiring caller free).
- Ensure no OS headers are transitively included by core/unicode/util.
- Ensure platform `#ifdef` exists only in platform backends and the selection TU.

