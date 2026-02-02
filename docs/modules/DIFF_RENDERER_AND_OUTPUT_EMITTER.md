# Module — Diff Renderer and Output Emitter

This module defines how framebuffer diffs are converted into terminal bytes and emitted to the platform backend.

## Goals

- Deterministic output for a given `(prev, next, caps, initial_term_state)`.
- Single flush per present: exactly one platform write call on success.
- No partial effects: failures do not write partial output.

## Components

### Diff renderer (`src/core/zr_diff.h`)

`zr_diff_render(prev, next, caps, initial_term_state, out_buf, out_cap, ...)` is a **pure** renderer:

- It does not mutate the input framebuffers.
- It writes `out_len` bytes to the caller-provided `out_buf` on success.
- If the output does not fit in `out_cap`, it returns `ZR_ERR_LIMIT` and reports `out_len = 0`.
- When scroll optimizations are enabled and `caps.supports_scroll_region == 1`, it may emit DECSTBM + SU/SD and redraw
  only newly exposed lines.

### Output emitter (`engine_present`, `src/core/zr_engine.h`)

`engine_present()` owns the platform flush policy:

- The engine allocates an engine-owned output buffer sized by `zr_limits_t.out_max_bytes_per_frame`.
- On success, `engine_present()` MUST call `plat_write_output()` exactly once with the full diff output bytes.
- On `ZR_ERR_LIMIT` (or any other failure), `engine_present()` MUST perform **zero** platform writes (no partial flush).
- Framebuffer swap (`prev ← next`) occurs only on success.
- When `caps.supports_sync_update == 1`, `engine_present()` may wrap the frame output in `ESC[?2026h` / `ESC[?2026l`.
