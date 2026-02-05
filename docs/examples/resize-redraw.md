# Resize + Redraw

Source: `examples/resize_redraw.c`

## Demonstrates

- extracting `ZR_EV_RESIZE` from packed event records
- updating wrapper viewport state from resize events
- rebuilding and presenting drawlists after size changes

## Run

```bash
./out/build/posix-clang-debug/zr_example_resize_redraw
```

## Key Takeaways

- wrappers should treat resize as event-driven state
- redraw should happen against latest known dimensions
- resize handling should stay robust under frequent terminal changes

## Related Reading

- [Rendering Model](../user-guide/rendering-model.md)
- [Internal Framebuffer Spec](../modules/FRAMEBUFFER_MODEL_AND_OPS.md)
