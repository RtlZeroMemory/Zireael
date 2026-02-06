# Input Echo

Source: `examples/input_echo.c`

## Demonstrates

- wrapper-side parsing of packed event batches
- handling of multiple event types (`KEY`, `TEXT`, `PASTE`, `MOUSE`, `RESIZE`, `TICK`, `USER`)
- rendering a rolling textual event log via drawlist commands

## Run

```bash
./out/build/posix-clang-debug/zr_example_input_echo
```

## Key Takeaways

- unknown event types should be skipped by record size
- event parsing must validate batch and record bounds first
- wrappers can treat event batch as transport and map into app-native events

## Related Reading

- [Event Batch Format](../abi/event-batch-format.md)
- [Input Model](../user-guide/input-model.md)
