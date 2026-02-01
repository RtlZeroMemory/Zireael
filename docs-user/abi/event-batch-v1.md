# Event batch v1

Events are written as a packed batch into a caller-provided buffer.

## Source of truth

- ABI types: `include/zr/zr_event.h`
- Spec: `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`

## Truncation policy (v1)

If the caller buffer cannot fit all events:

- the engine writes as many **complete** records as fit
- sets `ZR_EV_BATCH_TRUNCATED` in the batch header
- returns the bytes written as a success
