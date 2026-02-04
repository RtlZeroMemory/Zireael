# FAQ

## Is this a UI framework?

No. Zireael is a **low-level engine** meant to be embedded by a higher-level framework.

## What is “binary in / binary out”?

- Wrappers submit a versioned, little-endian **drawlist** byte stream for rendering.
- The engine returns a versioned **event batch** byte stream for input.

This keeps the public API small while still allowing high throughput.

## Does the engine allocate memory?

Yes — but the **engine owns its allocations**. Callers never free engine memory. Callers provide input/output buffers (drawlist bytes in, event batch bytes out).

## Where is the authoritative specification?

The implementation-ready specs live in `docs/` (see **Internal Specs** in the navigation). For FFI integration, start with:

- [ABI → ABI Policy](../abi/abi-policy.md)
- [ABI → C ABI Reference](../abi/c-abi-reference.md)

## Next steps

- [User Guide → Concepts](../user-guide/concepts.md)
- [ABI → Drawlist Format](../abi/drawlist-format.md)
- [ABI → Event Batch Format](../abi/event-batch-format.md)

