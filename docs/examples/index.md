# Examples

Examples are small C programs under `examples/` that exercise the public ABI.

## Build

Examples are enabled by default (`-DZIREAEL_BUILD_EXAMPLES=ON`).

```bash
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
```

## Next steps

- [Minimal Render Loop](minimal-render-loop.md)
- [Input Echo](input-echo.md)
- [Resize + Redraw](resize-redraw.md)

