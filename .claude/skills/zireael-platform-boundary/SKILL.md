---
name: zireael-platform-boundary
description: Enforce the strict platform boundary where core/unicode/util stay OS-header-free.
metadata:
  short-description: Platform interface + backend rules
---

## When to use

Use this skill when working on:

- the platform interface header (`src/platform/zr_platform.h`)
- Win32 or POSIX backend implementation
- event loop blocking/wake strategy
- terminal raw mode lifecycle and restoration

## Non-negotiable boundary rules

- `src/core`, `src/unicode`, `src/util` MUST NOT include OS headers (Win32 or POSIX).
- OS-specific code MUST be isolated to:
    - `src/platform/win32/`
    - `src/platform/posix/`
- Avoid `#ifdef` in core/unicode/util; allowed only in platform backends and minimal selection glue.

## Platform interface design

Goal: a core-facing platform interface that is:

- POD-only (fixed-width integer types; no OS types)
- opaque handle based (`plat_t*`)
- allocation-free in hot paths
- supports a "wake" primitive to interrupt blocking waits

Interface categories:

- **lifecycle**: create/destroy, enter/leave raw (idempotent best-effort restore)
- **I/O**: read input bytes, write output bytes (single flush per present)
- **wait/wake**: block until input or wake event, wake from other threads
- **caps/size**: get terminal size, capabilities (color/mouse/paste)

## Source of truth

- `docs/REPO_LAYOUT.md` — boundary rules and `#ifdef` policy
- `docs/modules/PLATFORM_INTERFACE.md` — core-facing interface spec
- `src/platform/zr_platform.h` — actual interface header

## Backend notes

**POSIX:**

- raw mode: termios; restore on exit; best-effort restore on signals
- blocking: `poll/select` on stdin + wake fd (eventfd/self-pipe)
- resize: SIGWINCH + ioctl(TIOCGWINSZ) when flagged

**Win32:**

- enable VT output (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`)
- prefer VT input bytes (`ENABLE_VIRTUAL_TERMINAL_INPUT`)
- blocking: wait on input handle + wake event handle
- restore modes on teardown; handle console close events

## Testing

Platform correctness is validated via integration tests:

- POSIX: PTY-based tests (headless)
- Windows: ConPTY-based tests where possible
