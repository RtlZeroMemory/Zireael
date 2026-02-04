# Input model

Terminal input bytes are parsed and normalized into engine events, then packed into a versioned **event batch**.

## Event batches

- `engine_poll_events()` writes a packed event batch into a caller-provided buffer.
- Wrappers parse it and translate to their own event model.

See: [ABI → Event Batch Format](../abi/event-batch-format.md).

## Bracketed paste

When enabled by the platform backend, terminal bracketed paste input is emitted as `ZR_EV_PASTE` records. Paste payloads are UTF-8 bytes and can be larger than “normal” key/text events, so wrappers should size their event buffer accordingly and handle truncation.

## Threading

Only `engine_post_user_event()` is intended to be thread-safe (used to wake an engine blocked in `engine_poll_events()`).

## Next steps

- [ABI → Event Batch Format](../abi/event-batch-format.md)
- [Internal Specs → Events + Packed ABI](../modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md)
- [Examples → Input Echo](../examples/input-echo.md)
