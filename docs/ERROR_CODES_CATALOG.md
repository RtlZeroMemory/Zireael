# Zireael — Error Codes Catalog (Locked)

This is the single source for `ZR_ERR_*` semantics.

## Global contract

- `ZR_OK == 0`
- Failures are negative `ZR_ERR_*` codes.
- Default rule: **no partial effects** on failure (do not partially mutate engine state or partially write caller outputs).
- The only permitted v1 “partial output” mode is **event batch truncation**, which is a successful return with `ZR_EV_BATCH_TRUNCATED` set in the batch header (not a negative error).

## Codes

Defined in `src/util/zr_result.h`:

- `ZR_OK (0)`: success
- `ZR_ERR_INVALID_ARGUMENT (-1)`: invalid args (NULL pointer where not allowed, impossible cap values, invalid enum, etc.)
- `ZR_ERR_OOM (-2)`: allocation failure
- `ZR_ERR_LIMIT (-3)`: output buffer too small or cap/limit exceeded
- `ZR_ERR_UNSUPPORTED (-4)`: unsupported version/feature/opcode
- `ZR_ERR_FORMAT (-5)`: malformed input bytes / framing violation
- `ZR_ERR_PLATFORM (-6)`: platform/backend failure (OS call failed; terminal/PTY not available; I/O wait/wake failed)

## Notes

- Parsers MUST be deterministic: the same invalid input must yield the same error code regardless of platform.
- If an API supports truncation as a success mode (currently: packed event batches), it MUST only emit complete records; no partial record bytes may be written.
