# Module — Event System and Packed Event ABI

The engine outputs events as a packed, versioned, self-framed batch into a caller-provided buffer.

## Batch format (v1)

Defined by `src/core/zr_event.h`:

- Batch begins with `zr_evbatch_header_t`.
- Records are framed by `zr_ev_record_header_t.size` and are 4-byte aligned.
- If output buffer is too small, the batch is truncated as a **success**: `ZR_EV_BATCH_TRUNCATED` is set and only
  complete records are emitted.

## Forward compatibility

- Unknown record types must be skippable by size.
- Record sizes must be validated against the batch `total_size`.

