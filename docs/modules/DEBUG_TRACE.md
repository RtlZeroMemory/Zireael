# Module — Debug Trace System

### Purpose

The debug trace system provides a **bounded, deterministic diagnostic log** that can be queried by wrappers at runtime.
It exists to make rendering and event/debugging investigations reproducible without guessing which subsystem failed.

Design goals:

- **Bounded memory**: fixed-capacity ring buffers; old records are overwritten.
- **Deterministic behavior**: stable ordering; no timestamps from wall-clock conversions beyond engine-provided clocks.
- **No per-frame heap churn**: recording does not allocate; all storage is pre-provisioned.
- **Wrapper-friendly**: records are POD with fixed-width integers; payloads can be exported as bytes.

### Public API Surface

Declared in `include/zr/zr_engine.h` and `include/zr/zr_debug.h`:

- `engine_debug_enable(e, cfg)` / `engine_debug_disable(e)`
- `engine_debug_query(e, query, out_headers, out_headers_cap, out_result)`
- `engine_debug_get_payload(e, record_id, out_payload, out_cap, out_size)`
- `engine_debug_get_stats(e, out_stats)`
- `engine_debug_export(e, out_buf, out_cap)`
- `engine_debug_reset(e)`

### Record Model

Each record is stored as:

- `zr_debug_record_header_t` (fixed size)
- `payload` (0..N bytes; size recorded in header)

Headers include:

- `record_id`: monotonic counter, increasing with each stored record.
- `timestamp_us`: microseconds since `engine_debug_enable()` (relative to engine start time configured for the trace).
- `frame_id`: engine-defined frame correlation id.
- `category`, `severity`, `code`: filterable fields for wrappers.

Payload interpretation is determined by `category` / `code`:

- For most categories, payloads are fixed-size structs defined in `include/zr/zr_debug.h`
  (`zr_debug_frame_record_t`, `zr_debug_drawlist_record_t`, etc.).
- When `zr_debug_config_t.capture_drawlist_bytes != 0`, the engine may emit an additional
  `ZR_DEBUG_CAT_DRAWLIST` record with `code == ZR_DEBUG_CODE_DRAWLIST_CMD` whose payload is a
  raw byte slice of the submitted drawlist (capped by `ZR_DEBUG_MAX_PAYLOAD_SIZE`).

### Storage and Eviction

Internally the system uses two rings:

- **Byte ring**: stores `(header + payload)` bytes for each record.
- **Index ring**: stores `(offset,size)` pairs so records can be located without scanning variable-length payloads.

Eviction policy:

- Records are evicted in FIFO order (oldest first).
- Eviction occurs when:
  - the byte ring lacks room for a new record, and/or
  - the index ring is full (must evict one oldest record to keep rings consistent).

`zr_debug_query_result_t.records_dropped` is a best-effort aggregate count of overwritten records since the last
enable/reset.

### Threading and Safety

- The trace implementation is **engine-thread only**; it is not internally synchronized.
- The trace never writes to caller buffers except through the explicit query/export APIs.
- All writes are performed via `memcpy` to avoid unaligned access.
