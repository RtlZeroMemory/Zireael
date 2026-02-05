# Examples

Examples are small C programs under `examples/` that exercise the public ABI directly.

## Included Programs

- `minimal_render_loop.c` - engine lifecycle + render loop
- `input_echo.c` - event batch parsing and rendering event summaries
- `resize_redraw.c` - resize handling and redraw behavior

## Build

```bash
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
```

## Run

```bash
./out/build/posix-clang-debug/zr_example_minimal_render_loop
./out/build/posix-clang-debug/zr_example_input_echo
./out/build/posix-clang-debug/zr_example_resize_redraw
```

## What To Learn From These Examples

- how to negotiate versions/config
- how to parse packed event records safely
- how to build valid drawlist payloads
- how to structure wrapper-controlled frame loops

## Next Steps

- [Minimal Render Loop](minimal-render-loop.md)
- [Input Echo](input-echo.md)
- [Resize + Redraw](resize-redraw.md)
- [ABI -> C ABI Reference](../abi/c-abi-reference.md)
