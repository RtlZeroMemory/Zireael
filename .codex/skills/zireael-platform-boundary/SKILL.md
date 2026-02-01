---
name: zireael-platform-boundary
description: Design/enforce the strict platform boundary: core/unicode/util stay OS-header-free; OS code lives in platform backends.
metadata:
  short-description: Platform interface + backend rules
---

## When to use

Use this skill when working on:

- the platform interface header(s)
- Win32 / POSIX backend implementation
- event loop blocking/wake strategy
- terminal raw mode lifecycle and restoration

## Non-negotiable boundary rules

- `src/core`, `src/unicode`, `src/util` must not include OS headers (Win32 or POSIX).
- OS-specific code must be isolated to:
  - `src/platform/win32`
  - `src/platform/posix`
- Avoid `#ifdef` in core/unicode/util; allowed only in platform backends and minimal selection glue.

## Platform interface design guidance

Goal: a core-facing platform interface that is:

- POD-only (fixed-width integer types; no OS types)
- opaque handle based (`plat_t*`)
- allocation-free in hot paths
- supports a “wake” primitive to interrupt blocking waits

Recommended interface categories (see internal docs if available):

- lifecycle: create/destroy, enter/leave raw (idempotent best-effort restore)
- I/O: read input bytes, write output bytes (single flush per present)
- wait/wake: block until input or wake event, wake from other threads
- resize/caps: get size, capabilities (color/mouse/paste)

## Backend notes (high level)

POSIX:

- raw mode: termios; restore on exit; best-effort restore on signals
- blocking: `poll/select` on stdin + wake fd (eventfd/self-pipe)
- resize: SIGWINCH + ioctl(TIOCGWINSZ) when flagged

Win32:

- enable VT output (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`)
- prefer VT input bytes (`ENABLE_VIRTUAL_TERMINAL_INPUT`) to unify parsing with POSIX
- blocking: wait on input handle + wake event handle
- restore modes on teardown; handle console close events via ctrl handler

## Integration testing requirement

Platform correctness is validated via integration tests (not pure unit tests):

- POSIX: PTY-based tests (headless)
- Windows: ConPTY-based tests where possible

