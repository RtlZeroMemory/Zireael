---
name: zireael-header-layering
description: Prevent include cycles and platform-boundary leaks by enforcing header layering.
metadata:
  short-description: Headers + include discipline
---

## When to use

Use this skill when:

- adding new headers or refactoring includes
- encountering circular dependencies
- tightening the platform boundary

## Source of truth

- `docs/REPO_LAYOUT.md` — dependency direction and `#ifdef` policy
- `docs/LIBC_POLICY.md` — stdlib usage constraints

## Include layering rules (must follow)

```
src/util/      → OS-header-free, no internal deps
      ↑
src/unicode/   → may include util; OS-header-free
      ↑
src/core/      → may include util, unicode, platform interface; OS-header-free
      ↑
src/platform/  → implements zr_platform.h; may include OS headers
```

## Specific rules

- `src/util/**` MUST be OS-header-free
- `src/unicode/**` may include `src/util/**` only; MUST be OS-header-free
- `src/core/**` may include util, unicode, and `zr_platform.h`; MUST be OS-header-free
- `src/platform/**` implements `zr_platform.h`; may include OS headers

## Best practices

- Prefer forward declarations in headers to avoid deep include trees
- Keep ABI-visible headers POD-only (fixed-width ints)
- Ensure no OS headers are transitively included by core/unicode/util
- Platform `#ifdef` exists only in platform backends and selection TU

## Checklist

- [ ] No OS headers in core/unicode/util
- [ ] No circular includes
- [ ] Forward declarations where possible
- [ ] ABI headers use fixed-width types only
