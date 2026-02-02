# Metrics (zr_metrics_t)

Zireael exposes a stable, append-only metrics snapshot via:

```c
zr_result_t engine_get_metrics(zr_engine_t* e, zr_metrics_t* out_metrics);
```

`zr_metrics_t` is **POD** (no pointers) and uses a **prefix-copy** contract so older bindings keep working when new fields are appended.

## Prefix-copy contract

The caller controls how many bytes the engine is allowed to write by setting `out_metrics->struct_size`:

- If `struct_size == 0`: copy **0 bytes** (no-op; returns `ZR_OK`).
- If `struct_size >= 4`: copy `min(struct_size, sizeof(zr_metrics_t))` bytes.
- If `0 < struct_size < 4`: invalid (`ZR_ERR_INVALID_ARGUMENT`).

When `struct_size != 0`, the engine overwrites the copied prefix so that:

- `out_metrics->struct_size == sizeof(zr_metrics_t)` (the engine’s full struct size)

This allows wrappers to:

- request only the prefix they understand
- still learn the engine’s full metrics struct size for debugging/telemetry

## C example (full struct)

```c
#include <stdio.h>

#include <zr/zr_engine.h>
#include <zr/zr_metrics.h>
#include <zr/zr_result.h>

static void dump_metrics(zr_engine_t* e) {
  zr_metrics_t m;
  m.struct_size = (uint32_t)sizeof(m);

  zr_result_t rc = engine_get_metrics(e, &m);
  if (rc != ZR_OK) {
    fprintf(stderr, "engine_get_metrics failed: %d\n", (int)rc);
    return;
  }

  fprintf(stderr, "fps=%u bytes_last=%u dirty_lines=%u\n",
          m.fps, m.bytes_emitted_last_frame, m.dirty_lines_last_frame);
}
```

## Wrapper pattern (prefix struct)

If your binding only needs a few fields, define a smaller struct that matches the prefix exactly and set `struct_size` to its size.

Example: a minimal prefix that can read `fps` and `bytes_emitted_last_frame`:

```c
typedef struct zr_metrics_prefix_v1_t {
  uint32_t struct_size;

  uint32_t negotiated_engine_abi_major;
  uint32_t negotiated_engine_abi_minor;
  uint32_t negotiated_engine_abi_patch;

  uint32_t negotiated_drawlist_version;
  uint32_t negotiated_event_batch_version;

  uint64_t frame_index;
  uint32_t fps;
  uint32_t _pad0;

  uint64_t bytes_emitted_total;
  uint32_t bytes_emitted_last_frame;
  uint32_t _pad1;
} zr_metrics_prefix_v1_t;
```

Call `engine_get_metrics(e, (zr_metrics_t*)&prefix)` (or the equivalent in your FFI) with `prefix.struct_size = sizeof(prefix)`.

## Notes

- Metrics are best-effort telemetry; treat timing fields as approximate.
- The struct is append-only. New fields must be added at the end, and older bindings can continue prefix-copying safely.
