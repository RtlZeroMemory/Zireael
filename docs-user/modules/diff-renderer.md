# Module: Diff renderer & output emission

The diff renderer converts framebuffer differences into terminal output bytes.

## Source of truth

- Internal headers: `src/core/zr_diff.h`
- Internal spec (normative): `docs/modules/DIFF_RENDERER_AND_OUTPUT_EMITTER.md`

## Guarantees

- Deterministic output for a given `(prev, next, config, caps)`.
- Single flush per present: exactly one platform write call on success.
- No partial effects: failures do not write partial output.

`engine_present()` owns the flush policy and only swaps framebuffers on success.
