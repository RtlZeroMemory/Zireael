# Release Model

Zireael uses a SemVer-based release model aligned with common open-source C library practices.

## Standards

- Semantic Versioning 2.0.0 for tag shape and compatibility intent.
- Keep a Changelog structure for human-readable release notes in `CHANGELOG.md`.
- Immutable tags once published.

## Tag Scheme

- Stable release: `vMAJOR.MINOR.PATCH` (example: `v1.2.0`)
- Pre-release: `vMAJOR.MINOR.PATCH-alpha.N`, `v...-beta.N`, `v...-rc.N`
  - examples: `v1.3.0-alpha.1`, `v1.3.0-beta.2`, `v1.3.0-rc.1`

`v`-prefixed tags are the only release trigger.

## Version Sources Of Truth

- Library version pins: `include/zr/zr_version.h`
- ABI and format policy: `docs/abi/versioning.md`, `docs/abi/abi-policy.md`
- Release notes: `CHANGELOG.md`

The release tag core version (`MAJOR.MINOR.PATCH`) must match the pinned library version in `include/zr/zr_version.h`.

## SemVer Mapping

- Patch: bug fixes and compatibility-safe behavior corrections.
- Minor: backward-compatible public additions (new APIs, additive capabilities, new negotiated format versions).
- Major: breaking C ABI or incompatible format/contract changes.

Drawlist/event format evolution remains explicitly versioned and negotiated at `engine_create()`.

## Release Automation Contract

The release workflow (`.github/workflows/release.yml`) validates:

- tag shape is valid SemVer (`vMAJOR.MINOR.PATCH[-PRERELEASE][+BUILDMETA]`)
- tag core version matches `include/zr/zr_version.h`
- `CHANGELOG.md` contains a heading for the exact release version

Pre-release tags are published as GitHub pre-releases automatically.

## Release Checklist

1. Update `include/zr/zr_version.h` when bumping library version.
2. Add/update the matching release entry in `CHANGELOG.md`.
3. Ensure docs are synced (`docs/VERSION_PINS.md`, ABI/versioning pages).
4. Run:
   - `python3 scripts/check_version_pins.py`
   - `python3 scripts/check_release_tag.py vX.Y.Z[-PRERELEASE]`
   - CI and tests (`ctest --output-on-failure`)
5. Create and push annotated tag `vX.Y.Z...`.

## Related

- [Versioning](abi/versioning.md)
- [ABI Policy](abi/abi-policy.md)
- [Maintainers](maintainers.md)
