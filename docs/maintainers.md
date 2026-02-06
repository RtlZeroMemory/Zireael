# Maintainers: Keeping Docs Healthy

Zireael publishes wrapper-facing docs and keeps implementation-ready internal specs in the same repository.

## Source-Of-Truth Rule

If wrapper-facing docs disagree with internal specs under `docs/`, internal specs are authoritative.

## Required Consistency Checks

- version pins in `include/zr/zr_version.h` match docs/changelog references
- release tags follow SemVer and map to pinned version macros
- wrapper-facing API docs match current public headers under `include/zr/`
- examples compile and reflect current supported versions
- navigation has no dead links

## Release-Time Docs Checklist

When ABI or format behavior changes:

- update `include/zr/zr_version.h`
- update `docs/VERSION_PINS.md`
- update `docs/abi/versioning.md`
- update `docs/release-model.md` when release policy/channels change
- update affected ABI pages (`c-abi-reference`, format pages)
- update `CHANGELOG.md`
- run strict docs build

## Recommended CI Gates

- `bash scripts/guardrails.sh`
- `python3 scripts/check_version_pins.py`
- `python3 scripts/check_release_tag.py vX.Y.Z[-PRERELEASE]`
- `bash scripts/docs.sh build`
- relevant CTest preset(s)

## Documentation Quality Checklist

- pages explain contracts and failure modes, not just happy paths
- examples include realistic wrapper guidance
- public API docs include ownership/threading/return semantics
- internal references are linked rather than duplicated inconsistently

## Related Docs

- [Internal docs index](00_INDEX.md)
- [ABI policy](abi/abi-policy.md)
- [Release model](release-model.md)
- [Contributing](https://github.com/RtlZeroMemory/Zireael/blob/main/CONTRIBUTING.md)
