# Maintainers: keeping docs healthy

Zireael keeps both wrapper-facing docs (this site) and implementation-ready internal specs under `docs/`.

## Rules of the road

- If wrapper-facing docs and internal specs disagree, treat internal specs as authoritative and fix the wrapper-facing page.
- Prefer linking to internal specs for low-level details rather than duplicating byte layouts in multiple places.
- Keep all version pins consistent with `include/zr/zr_version.h`.
- Ensure example code uses **only** public headers (`include/zr/...`) and matches the documented versions.

## Review checklist

- `mkdocs build --strict` passes.
- No broken links in nav.
- Any new ABI/format version bump updates:
  - `include/zr/zr_version.h`
  - `docs/VERSION_PINS.md`
  - `docs/abi/versioning.md`
  - `CHANGELOG.md`

## CI hooks

CI should treat these as required:

- guardrails
- docs build
- version drift check (pins vs docs)

