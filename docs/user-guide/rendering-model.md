# Rendering model

Zireael maintains a logical framebuffer, diffs frames, and emits terminal escape sequences.

## Framebuffers and diff

At a high level:

1. Drawlist executes into a “next” framebuffer.
2. `engine_present()` diffs “prev” vs “next”.
3. The diff renderer emits output and flushes once.
4. Buffers swap.

## Partial redraw and damage

To keep terminal I/O low, the diff renderer tracks damage and may:

- skip unchanged regions
- coalesce damage rectangles
- use scroll-region optimizations when supported

## Output coalescing

The engine buffers all output for a present into an internal output buffer and performs a **single write**.

## Next steps

- [ABI → Drawlist Format](../abi/drawlist-format.md)
- [Internal Specs → Diff + Output](../modules/DIFF_RENDERER_AND_OUTPUT_EMITTER.md)
- [Examples → Minimal Render Loop](../examples/minimal-render-loop.md)

