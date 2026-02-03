# Module — Diagnostics: Metrics and Debug Overlay

This module defines the engine’s **diagnostics surfaces** that are safe for wrappers and deterministic for tests:

- `zr_metrics_t` — ABI-safe metrics snapshot struct
- debug overlay renderer — a small, bounded ASCII overlay rendered into the framebuffer
- debug trace system — bounded diagnostic record ring buffer (see `docs/modules/DEBUG_TRACE.md`)

## Metrics ABI (`zr_metrics_t`)

Defined in `src/core/zr_metrics.h`.

### ABI requirements

- **POD-only:** fixed-width integers only; **no pointers**.
- **Append-only:** new fields MUST be appended at the end to keep older callers compatible.
- **Prefix-copy handshake:** the first field is `struct_size` (bytes).

### Prefix-copy contract

`engine_get_metrics(zr_engine_t*, zr_metrics_t* out)` uses **prefix-copy** based on the caller-provided capacity:

- Caller sets `out->struct_size` to the number of bytes they can receive (possibly `0`).
- Engine copies `min(out->struct_size, sizeof(zr_metrics_t))` bytes from a snapshot into `out`.
- If `out->struct_size == 0`, the engine performs a 0-byte copy (no-op).
- For ABI sanity, `out->struct_size` MUST be either `0` or at least `sizeof(uint32_t)`.

### Size discovery

When a non-zero prefix copy occurs, the copied prefix includes the `struct_size` field, so the engine can report the
current struct size (`sizeof(zr_metrics_t)`) back to the caller. This allows wrappers to detect newer versions and
allocate a larger struct if desired.

## Damage summary metrics

The diff/present path records a damage summary for the last successfully presented frame:

- `damage_rects_last_frame` — number of coalesced damage rectangles rendered/emitted.
- `damage_cells_last_frame` — number of character cells covered by those rectangles.
- `damage_full_frame` — `1` when the engine treated the frame as “full damage” (e.g. due to a fullscreen operation or
  a cap/overflow fallback), else `0`.

These fields are **append-only** and are prefix-copied like the rest of `zr_metrics_t`.

## Debug overlay renderer

Defined in `src/core/zr_debug_overlay.h` / `src/core/zr_debug_overlay.c`.

### Bounds and determinism

- Renders a **bounded** ASCII overlay into the framebuffer, clipped to:
  - max rows: `ZR_DEBUG_OVERLAY_MAX_ROWS`
  - max cols: `ZR_DEBUG_OVERLAY_MAX_COLS`
- Never allocates.
- Output is deterministic for a given `zr_metrics_t` input.

### Wide-glyph safety

The framebuffer stores wide glyphs as a **lead cell** followed by a **continuation cell**
(`width == 2` lead, then `width == 0` continuation with `glyph_len == 0`). The overlay MUST NOT split a wide glyph across
the overlay boundary:

- If writing into a continuation cell, the overlay clears the lead + continuation together (when both are within the
  overlay region).
- If overwriting a lead cell whose next cell is a continuation, the overlay clears the continuation cell too (when it is
  within the overlay region).
- If a write would split a wide glyph across the right edge of the overlay region, the overlay skips that write and
  leaves the glyph intact.
