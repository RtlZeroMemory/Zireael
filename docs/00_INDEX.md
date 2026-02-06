# Zireael - Internal Docs Index

`docs/` is the normative specification set for Zireael implementation behavior.
If code conflicts with these docs, fix the code.

## Recommended Reading Order

1. `docs/CODE_STANDARDS.md`
2. `docs/SAFETY_RULESET.md`
3. `docs/LIBC_POLICY.md`
4. `docs/ERROR_CODES_CATALOG.md`
5. `docs/VERSION_PINS.md`
6. `docs/HEADER_LAYERING.md`
7. `docs/GOLDEN_FIXTURE_FORMAT.md`
8. `docs/REPO_LAYOUT.md`
9. `docs/BUILD_TOOLCHAINS_AND_CMAKE.md`
10. `docs/modules/` (module-level implementation contracts)

## Module Specs

- `docs/modules/CONFIG_AND_ABI_VERSIONING.md`
- `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md`
- `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`
- `docs/modules/FRAMEBUFFER_MODEL_AND_OPS.md`
- `docs/modules/DIFF_RENDERER_AND_OUTPUT_EMITTER.md`
- `docs/modules/PLATFORM_INTERFACE.md`
- `docs/modules/UNICODE_TEXT.md`
- `docs/modules/TESTING_GOLDENS_FUZZ_INTEGRATION.md`
- `docs/modules/DEBUG_TRACE.md`
- `docs/modules/DIAGNOSTICS_METRICS_DEBUG_OVERLAY.md`

## Related Wrapper-Facing Pages

- `docs/abi/abi-policy.md`
- `docs/abi/c-abi-reference.md`
- `docs/abi/drawlist-format.md`
- `docs/abi/event-batch-format.md`
- `docs/release-model.md`

## Reference

- Wrapper/FFI integration notes: `docs/ABI_REFERENCE.md`
