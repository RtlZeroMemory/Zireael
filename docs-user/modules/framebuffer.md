# Module: Framebuffer model

Zireael renders into an in-memory framebuffer (character-cell grid). Rendering
is a two-stage process:

1. Execute the drawlist into the “next” framebuffer.
2. Diff “prev” vs “next” to produce terminal output bytes.

## Source of truth

- Internal headers: `src/core/zr_framebuffer.h`, `src/core/zr_fb.h`
- Internal spec (normative): `docs/modules/FRAMEBUFFER_MODEL_AND_OPS.md`

## Key properties

- The framebuffer is designed for deterministic diffing and minimal output.
- Engine maintains two buffers (prev/next) and swaps only on successful present.
