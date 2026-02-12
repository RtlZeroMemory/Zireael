# Changelog

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

## 1.3.1 — 2026-02-12

### Added

- tests: expanded terminal edge-case coverage for higher CSI-tilde function keys, additional SGR mouse variants, unicode ZWJ/VS16 width policy vectors, wrap/tab boundary behavior, and mixed invalid/valid UTF-8 progression.
- tests: added Win32 ConPTY integration coverage for capability overrides (color clamp matrix, focus/output-writable overrides, and SGR attribute-mask parsing/precedence).

### Changed

- core: focus key events (`ZR_KEY_FOCUS_IN` / `ZR_KEY_FOCUS_OUT`) are now gated by runtime config and backend capability before event packing.
- tests: strengthened drawlist/diff limit and terminal-state assertions, including ANSI16 SGR emission and cursor-hide VT output vectors.

## 1.3.0 — 2026-02-12

### Added

- input: normalized focus-in/focus-out handling via `ESC [ I` / `ESC [ O` and new public key enums for focus events.
- input: CSI-u and modifyOtherKeys parsing coverage with deterministic fallback behavior for malformed/partial sequences.
- platform: new `ZIREAEL_CAP_FOCUS_EVENTS` and `ZIREAEL_CAP_SGR_ATTRS_MASK` capability overrides.

### Changed

- platform: expanded truecolor and terminal capability heuristics across POSIX and Win32 backends.
- platform: focus event mode is now enabled/disabled with raw-mode enter/leave when supported.
- diff: attribute emission now respects backend-reported `sgr_attrs_supported` instead of assuming all SGR attrs.

## 1.2.5 — 2026-02-12

### Changed

- core/platform: tightened terminal-state and capability handling across diff, input parsing, and backend capability reporting for edge-case resilience.
- platform: raw-mode leave now performs explicit scroll-region and SGR resets on POSIX and Win32 paths.

### Fixed

- core: force cursor-position re-sync (`CUP`) when cursor position validity is unknown, including no-damage frames with cursor position set to “do not change”.
- core: bracketed paste parsing now respects runtime config and backend capability gates.
- platform: `color_mode` is backend-detected and clamped against wrapper request instead of mirroring requested mode.

## 1.2.4 — 2026-02-11

### Changed

- core: refactored drawlist `DRAW_TEXT_RUN` validation into focused helpers for span-table reads, blob framing checks, and per-segment slice validation (no behavior change).
- docs: synchronized drawlist module/ABI pages with the deterministic three-phase `DRAW_TEXT_RUN` blob validation flow.

### Fixed

- unicode: treat keycap emoji sequences (`[0-9#*] U+FE0F? U+20E3`) as emoji in grapheme width policy so wide terminals stay cell-aligned.

## 1.2.3 — 2026-02-06

### Added

- core: added targeted unit coverage for diff hotpath telemetry (sweep vs damage selection, scroll-opt attempt/hit, and collision-guard counters).

### Changed

- core: phase 2 diff hotpath tuning now reuses row hashes across committed frames, uses deterministic adaptive sweep thresholds, and applies row-indexed damage coalescing to reduce coalescing scan cost.
- docs: expanded diff renderer module documentation with row-hash reuse lifecycle, adaptive threshold policy, and internal telemetry semantics.

### Fixed

- core: invalidates reused row-hash cache state on present failure paths (render/write error before commit) so failure recovery cannot reuse transient indexed-coalescing scratch state.
- core: paste enqueue allocation now returns `ZR_ERR_LIMIT` instead of asserting when payload storage cannot be reserved, preserving deterministic bounded failure behavior.

## 1.2.2 — 2026-02-06

### Added

- Perf benchmark target `zireael_perf_diff_hotpath` for repeatable diff/write/bytes measurements across sparse, dense, scroll-like, and style-churn scenarios.

### Changed

- core: diff renderer now supports per-line hash/dirty cache, adaptive sparse-vs-sweep selection, and improved scroll detection heuristics to reduce hot-path work.
- core: present path now records diff and write timing metrics to make regressions observable in CI and local profiling.
- win32: output writable waiting now handles pipe-like handles conservatively and reports unsupported wait capability explicitly.

### Fixed

- core: damage-span coalescing now preserves correctness when incoming dirty spans are not x-sorted.
- core: SGR delta emission now falls back cleanly so style-only transitions never emit partial `ESC[` control prefixes.
- tests/perf: removed 128-bit division usage that broke clang-cl linking (`__udivti3`) and replaced it with overflow-safe integer mean math.

## 1.2.1 — 2026-02-06

### Added

- Integration coverage for cross-thread `engine_post_user_event()` wake behavior on POSIX PTY and Win32 ConPTY paths.
- Optional ThreadSanitizer support in build/CI (`ZIREAEL_SANITIZE_THREAD`, `posix-clang-tsan` presets, and Linux TSan workflow job).

### Changed

- Clarified teardown/threading contract docs for `engine_destroy()` and `engine_post_user_event()` across ABI reference and FAQ pages.
- Logger sink installation/write path now snapshots sink state under internal synchronization before calling callbacks.

### Fixed

- core: closed `engine_destroy()` vs `engine_post_user_event()` lifetime race by gating new post calls during teardown and draining in-flight post calls before free.
- posix: removed cross-thread data races in SIGWINCH/self-pipe global state (`wake_write_fd`, pending flag) using C11 atomics.
- integration tests: stabilized posted-user-event detection under unrelated resize/tick noise for deterministic CI behavior.

## 1.2.0 — 2026-02-06

### Added

- Release automation SemVer/tag guard script and workflow validation to enforce tag/version/changelog consistency before publish.

### Changed

- Documented project lifecycle status as **alpha** in wrapper-facing docs and release guidance.
- Hardened fuzz smoke corpus coverage and warning-as-error parity across Linux and Windows CI toolchains.

### Fixed

- posix: signal-safety UB in SIGWINCH handler — previous-handler chaining now uses only `sig_atomic_t` and async-signal-safe state.
- core: `engine_create()` now rejects `wait_for_output_drain=1` early when the backend lacks `supports_output_wait_writable`, instead of failing every `engine_present()` call.
- core: `engine_set_config()` now also rejects enabling `wait_for_output_drain` on unsupported backends, preserving no-partial-effects runtime behavior.
- win32: mouse tracking now enables drag (`?1002h`) and any-motion (`?1003h`) modes for parity with POSIX backend.
- debug trace: hardened counter overflow handling and expanded tests/docs for trace record query behavior.

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
