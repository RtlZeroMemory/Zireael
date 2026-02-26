# Rendering Model

Zireael renders by executing drawlist commands into an internal framebuffer, diffing against the previous frame, then emitting minimal terminal bytes.

## Pipeline

1. Wrapper submits drawlist bytes.
2. Engine validates drawlist format and limits.
3. Drawlist executes into a staging framebuffer.
4. On success, staging framebuffer becomes next framebuffer.
5. `engine_present()` diffs previous vs next framebuffer.
6. Engine emits output bytes and performs one platform write.
7. Framebuffers swap; metrics advance.

## No-Partial-Effects Contract

- Invalid drawlist input fails before committed frame state changes.
- Failed present/write does not advance presented-frame metrics/state.

This makes wrapper retry and error handling deterministic.

## Diff and Damage

Diff output minimizes terminal I/O using:

- changed-cell detection between prev/next framebuffers
- damage rectangle tracking/coalescing
- scroll-region optimization when backend capabilities allow

Result metrics include dirty/damage counts for the last frame.

## Cursor Behavior

Drawlist v1 supports explicit cursor control (`ZR_DL_OP_SET_CURSOR`).

- cursor state is part of terminal emission behavior
- it does not mutate glyph content in framebuffer cells

Drawlist v1 also supports overlap-safe framebuffer rectangle copies
(`ZR_DL_OP_BLIT_RECT`) to express scroll/move style updates without redraw.

## Output Buffering and Flush

- Output is assembled into an internal bounded buffer.
- Successful present performs a single platform write.
- Optional sync-update wrapping can be applied when capability is detected.

## Performance Guidance For Wrappers

- avoid sending full-frame redraw drawlists when not needed
- keep drawlist payloads compact (strings/blobs reused where possible)
- monitor metrics (`bytes_emitted_last_frame`, damage fields)

## Related Specs

- [ABI -> Drawlist Format](../abi/drawlist-format.md)
- [Internal -> Diff + Output](../modules/DIFF_RENDERER_AND_OUTPUT_EMITTER.md)
- [Internal -> Framebuffer](../modules/FRAMEBUFFER_MODEL_AND_OPS.md)
