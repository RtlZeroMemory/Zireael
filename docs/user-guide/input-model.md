# Input Model

Zireael reads terminal input bytes, normalizes them into structured events, and packs them into a versioned event batch for wrappers.

## Event Flow

1. Platform backend reads raw terminal input bytes.
2. Core parser decodes supported key/mouse/paste/control sequences.
3. Events are queued in engine-owned structures.
4. `engine_poll_events()` serializes queued events into caller buffer.

## Event Batch Contract

- packed little-endian format (`zr_event.h`)
- record framing by explicit size
- 4-byte record alignment
- forward-compatible skipping of unknown record types

See [ABI -> Event Batch Format](../abi/event-batch-format.md).

Supported key-sequence normalization includes common CSI/SS3 navigation keys,
xterm focus in/out (`CSI I` / `CSI O`), SGR mouse, and CSI-based extended key
forms (such as `CSI ... u` and `CSI 27;...~`).

## Timeout and Tick Behavior

`engine_poll_events(timeout_ms, ...)`:

- may block up to `timeout_ms` when queue is empty
- may return `0` on timeout
- may include periodic `ZR_EV_TICK` records for cadence

## Resize Behavior

- Engine enqueues an initial `ZR_EV_RESIZE` during create.
- Resize changes are emitted as `ZR_EV_RESIZE` records when detected.

Wrappers should treat resize as authoritative viewport signal.

## Bracketed Paste

When enabled/supported:

- paste content is emitted as `ZR_EV_PASTE`
- payload includes declared byte length + UTF-8 bytes
- wrapper must validate payload size before reading

## User Events and Wakeups

`engine_post_user_event()` allows wrappers/other threads to enqueue app-defined events.

- payload is copied by engine
- engine performs best-effort wake for blocked poll

## Buffer Sizing Guidance

- start with at least 4 KiB event buffer
- increase for paste-heavy or mouse-heavy workloads
- monitor `ZR_EV_BATCH_TRUNCATED` to detect pressure

## Next Steps

- [ABI -> Event Batch Format](../abi/event-batch-format.md)
- [Examples -> Input Echo](../examples/input-echo.md)
- [Internal -> Event System + ABI](../modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md)
