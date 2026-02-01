# Module: Event system & packed event batch ABI

Zireael parses platform input into normalized internal events and writes a
packed event batch into a caller-provided output buffer.

## Source of truth

- ABI structs: `include/zr/zr_event.h`
- Engine implementation: `src/core/zr_event_queue.c`, `src/core/zr_event_pack.c`
- Internal spec (normative): `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`

## Truncation behavior (v1)

If the caller buffer cannot fit all events:

- the engine writes as many complete records as fit
- sets `ZR_EV_BATCH_TRUNCATED` in the batch header
- returns the bytes written as a success
