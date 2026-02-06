# Minimal Render Loop

Source: `examples/minimal_render_loop.c`

## Demonstrates

- `engine_create` / `engine_destroy`
- event polling into caller buffer
- drawlist submission and present
- exit-on-escape logic via parsed event batch

## Run

```bash
./out/build/posix-clang-debug/zr_example_minimal_render_loop
```

## Key Takeaways

- keep event parsing bounds-safe
- keep drawlist production deterministic and reserved fields zeroed
- handle negative results immediately

## Related Reading

- [Quickstart](../getting-started/quickstart.md)
- [C ABI Reference](../abi/c-abi-reference.md)
- [Event Batch Format](../abi/event-batch-format.md)
