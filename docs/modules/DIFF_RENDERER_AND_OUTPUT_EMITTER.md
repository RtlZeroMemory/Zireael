# Module — Diff Renderer and Output Emitter

This module defines how framebuffer diffs are converted into terminal bytes and emitted to the platform backend.

## Goals

- Deterministic output for a given `(prev, next, caps, initial_term_state)`.
- Single flush per present: exactly one platform write call on success.
- No partial effects: failures do not write partial output.

## Components

### Diff renderer (`src/core/zr_diff.h`)

`zr_diff_render(prev, next, caps, initial_term_state, desired_cursor_state, lim, scratch_damage_rects, ..., out_buf, out_cap, ...)`
is a **pure** renderer:

- It does not mutate the input framebuffers.
- It writes `out_len` bytes to the caller-provided `out_buf` on success.
- If the output does not fit in `out_cap`, it returns `ZR_ERR_LIMIT` and reports `out_len = 0`.
- When scroll optimizations are enabled and `caps.supports_scroll_region == 1`, it may emit DECSTBM + SU/SD and redraw
  only newly exposed lines.

#### Damage rectangles

The diff renderer computes a bounded, coalesced set of **damage rectangles** (cell-space) for the frame:

- Rectangles are derived from `prev → next` cell differences.
- The renderer emits bytes only for cells within the damaged rectangles.
- Wide-glyph safety is preserved: damaged regions are expanded as needed so the renderer never emits only half of a wide glyph.
- The rectangle list is cap-bounded by `zr_limits_t.diff_max_damage_rects`. When exceeded, the renderer falls back to a
  “full damage” mode for that frame and reports it via metrics (`damage_full_frame = 1`).

#### Optional row-cache scratch

`zr_diff_render_ex(...)` accepts optional caller-owned row scratch (`zr_diff_scratch_t`):

- `prev_row_hashes[]`, `next_row_hashes[]`, `dirty_rows[]` are sized to at least framebuffer row count.
- `prev_hashes_valid` lets callers reuse previously computed `prev_row_hashes[]` across frames.
- The renderer precomputes per-row fingerprints and dirty hints once per frame (or reuses valid previous hashes).
- If scratch is not supplied, rendering remains correct and deterministic; only the optimization path is disabled.

#### Adaptive render path

When row-cache data is available, the renderer can choose between:

- sparse damage-rectangle rendering (default path for low dirty density)
- per-row sweep rendering (selected when dirty-row density crosses an adaptive threshold)

Adaptive threshold policy is intentionally simple:

- starts from a fixed base dirty-row percentage
- applies bounded adjustments for very small frames, wide frames, and very-dirty frames
- remains deterministic for identical inputs

When rendering through damage rectangles, coalescing uses a row-indexed active-rectangle walk to avoid the legacy
`rows * rect_count` full scan while preserving deterministic interval ordering.

Both paths preserve:

- grapheme/wide-cell safety
- single-output-buffer semantics (`ZR_ERR_LIMIT` on truncation, `out_len = 0` on failure)
- deterministic output for the same `(prev, next, caps, initial_term_state)`
- no per-frame heap churn

#### Internal telemetry counters

The diff renderer records internal counters for diagnostics/debug trace plumbing:

- sweep path usage vs damage path usage
- scroll optimization attempted/hit flags
- hash collision-guard hits (equal-hash rows that required exact byte compare fallback)

#### SGR emission policy

Style changes use a deterministic delta-then-fallback policy:

- add-only attr/color changes emit compact delta SGR parameters
- any attribute clear falls back to absolute reset-based SGR (`0;...m`) to avoid backend-specific clear-code assumptions
- no partial SGR sequences are emitted; each emitted CSI is syntactically complete

### Cursor control

Cursor control is applied as part of output emission:

- The diff renderer tracks terminal state beyond `(cursor_x, cursor_y)` to avoid redundant sequences.
- After diff bytes are produced, the present path may append cursor-control sequences (visibility + shape + blink and an
  optional final cursor position) based on the engine-owned desired cursor state.

### Output emitter (`engine_present`, `src/core/zr_engine.h`)

`engine_present()` owns the platform flush policy:

- The engine allocates an engine-owned output buffer sized by `zr_limits_t.out_max_bytes_per_frame`.
- On success, `engine_present()` MUST call `plat_write_output()` exactly once with the full diff output bytes.
- On `ZR_ERR_LIMIT` (or any other failure), `engine_present()` MUST perform **zero** platform writes (no partial flush).
- Framebuffer swap (`prev ← next`) occurs only on success.
- When `caps.supports_sync_update == 1`, `engine_present()` may wrap the frame output in `ESC[?2026h` / `ESC[?2026l`.

### Optional backpressure (wait-for-output-drain)

When enabled by config, `engine_present()` performs a bounded wait for output to become writable before emitting bytes:

- The engine calls `plat_wait_output_writable(plat, timeout_ms)` once per present.
- The default timeout is derived from `target_fps` (one frame budget in milliseconds, clamped to at least 1ms).
- On timeout, `engine_present()` returns `ZR_ERR_LIMIT` and performs **zero** platform writes.
