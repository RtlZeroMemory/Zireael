# Changelog

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

## 1.2.0-rc5 — 2026-02-03

### Fixed

- posix: fall back to `/dev/tty` when stdio isn't a tty (#17)

## 1.2.0-rc4 — 2026-02-03

### Fixed

- posix: decode function keys and enable richer mouse tracking (#16)

## 1.2.0-rc3 — 2026-02-03

### Fixed

- core: treat SGR mouse motion-without-buttons (e.g. `b=35` with any-event tracking) as `ZR_MOUSE_MOVE` rather than a spurious `ZR_MOUSE_UP`.

## 1.2.0-rc2 — 2026-02-03

### Fixed

- core: parse common CSI/SS3 key sequences (including parameterized arrows like `ESC [ 1 ; 5 A`) so ArrowUp/Down/Left/Right work reliably across terminals.
- core: parse SGR mouse (`DECSET ?1006`) sequences into `ZR_EV_MOUSE` events (down/up/drag/wheel) for interactive UIs.
- win32: honor `KEY_EVENT_RECORD.wRepeatCount` when translating console input records into the byte stream consumed by the core parser.

## 1.2.0-rc1 — 2026-02-03

### Added

- engine: debug trace system (`engine_debug_*`) for capturing and querying diagnostic records (frames, drawlists, and optional drawlist byte capture) via a bounded ring buffer.
- engine: bump Engine ABI pins to `1.1.0` for the new debug API surface (wrappers must request the updated ABI in `zr_engine_config_t`).

## 1.1.1-rc4 — 2026-02-03

### Fixed

- engine: enqueue an initial `ZR_EV_RESIZE` during `engine_create()` so wrappers can size their viewport correctly on the first frame (avoids stale wrapper-side terminal size at startup).
- posix: enable synchronized output (`DEC ?2026`) in Rio terminal for tear-free full-screen updates.

## 1.1.1-rc3 — 2026-02-03

### Fixed

- engine: detect terminal resize even when `engine_poll_events(0, ...)` times out (best-effort size check on timeout to avoid missed resizes when SIGWINCH delivery is disrupted).

## 1.1.1-rc2 — 2026-02-03

### Fixed

- engine: emit `ZR_EV_TICK` periodically from `engine_poll_events()` (bounded by `target_fps`) so wrappers can rely on non-zero `dt_ms` ticks even without input.
- engine: when a tick is due, drain immediately-available input before emitting the tick so input is not delayed by one poll.

## 1.1.1-rc1 — 2026-02-02

### Fixed

- Go PoC: sync `zr_engine_config_t` mirror with `zr_limits_t.diff_max_damage_rects` and `wait_for_output_drain`.
- Go PoC runner: force a rebuild (`go run -a`) to avoid stale cgo builds after header/ABI changes.

## 1.1.0 — 2026-02-02

### Added

**Drawlist v2**
- `SET_CURSOR` opcode for explicit cursor control (position, shape, visibility, blink)
- Cursor shapes: block (0), underline (1), bar (2)
- Backward compatible: v1 drawlists still accepted

**Synchronized Output**
- Frames wrapped in `CSI ?2026h` / `CSI ?2026l` for tear-free rendering
- Auto-enabled when terminal supports it (`supports_sync_update` capability)

**Scroll Region Optimization**
- Detects vertical scroll patterns in framebuffer changes
- Uses `DECSTBM + SU/SD` terminal sequences for bulk scrolling
- Reduces output from O(rows) to O(1) for scroll operations
- Controlled via `enable_scroll_optimizations` config flag

**Damage Rectangle Tracking**
- Tracks changed regions as coalesced rectangles
- Diff renderer skips unchanged areas
- Configurable via `diff_max_damage_rects` limit
- Metrics: `damage_rects_last_frame`, `damage_cells_last_frame`, `damage_full_frame`

**Enhanced Capability Detection**
- `supports_sync_update` — synchronized output protocol
- `supports_scroll_region` — DECSTBM scroll regions
- `supports_cursor_shape` — DECSCUSR cursor shapes
- `supports_output_wait_writable` — non-blocking output readiness

**Metrics Additions**
- `damage_rects_last_frame` — rectangle count in last frame
- `damage_cells_last_frame` — total damaged cells
- `damage_full_frame` — flag indicating full redraw

### Changed

- `zr_diff_render()` signature extended for damage tracking and cursor state
- `zr_limits_t` now includes `diff_max_damage_rects`
- `plat_caps_t` extended with new capability fields

## 1.0.0-rc1 — 2026-02-02

- Initial public preview release (Engine ABI v1; Drawlist v1; Event Batch v1).
- Multi-platform release artifacts for Linux/macOS/Windows.

## 1.0.0 — 2026-02-01

- Initial public engine ABI v1 (drawlist v1, event batch v1).
