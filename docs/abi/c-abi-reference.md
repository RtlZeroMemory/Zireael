# C ABI reference

This is a wrapper-facing index of the public headers under `include/zr/`.

For the most detailed API reference (including doc comments), see: **[API reference (Doxygen)](../api.md)**.

## Key entrypoints

Most wrappers only need:

- `engine_create()` / `engine_destroy()`
- `engine_poll_events()` (packs event batches into a caller buffer)
- `engine_submit_drawlist()` (validates + executes drawlist bytes)
- `engine_present()` (diff + single flush)

## Ownership (reminder)

- The engine owns its allocations.
- Callers provide drawlist bytes and event output buffers.

## Errors

Most functions return `zr_result_t` (`0 = OK`, negative failures). `engine_poll_events()` returns bytes written (>= 0) on success.

## Next steps

- [ABI Policy](abi-policy.md)
- [Drawlist Format](drawlist-format.md)
- [Event Batch Format](event-batch-format.md)
