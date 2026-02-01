C Core Engine — Master Spec (Locked)

Purpose
This document is the single source of truth for the C core engine: required features, portability rules, ABI, build/tooling, data structures, allocator/ownership model, and code standards.
OpenAI CODEX will use this document to generate concrete separated .md docs and implementation plans.

0) Non-negotiables

Engine is cross-platform: Windows + Linux + macOS.

Engine owns terminal IO, input decoding, scheduling, rendering, diff output.

Wrapper submits one binary drawlist per frame and consumes normalized events.

No OS headers in core modules.

Arena allocator + strict ownership model from day 1 (no later refactor).

1) Compiler / Toolchains

macOS: Apple Clang

Linux: Clang + GCC in CI

Windows: clang-cl primary + (optional) MinGW-w64 GCC secondary

CI expectation: Matrix builds across these toolchains to ensure portability.

2) Portability & Project Structure
2.1 Platform boundary rules

Hard platform boundary:

/platform/win32

/platform/posix

Core must not include OS headers (windows.h, termios.h, etc).

#ifdef _WIN32 is allowed only in:

/platform/*

build selection glue (e.g., one small platform_select.c)

2.2 Suggested module layout
/src
  /core        (engine loop, events, framebuffer, diff, drawlist execution)
  /unicode     (utf8 decode, grapheme, width, wrapping/measurement)
  /platform
    /win32     (VT enable, console IO, resize, input)
    /posix     (termios, poll/select, signals, resize)
  /util        (arena, vec, ring, strings, logging, asserts, caps)

3) Build & Packaging

Build system: CMake

Include:

CMake presets (debug/release, clang/gcc/msvc)

CI matrix

Outputs:

static library (required)

shared library (optional/configurable)

minimal example app (required)

tests/fuzz targets (recommended)

4) Data Structures (Required)

Implement in /util:

arena allocator (required)

vec (dynamic array)

ring (ring buffer; used for bounded event queue)

string_view (ptr + len, no null termination implied)

string_builder (append-only buffer builder)

Optional acceleration (only if needed): uthash or stb_ds (portable single-header).

5) Unicode & Terminal Correctness

Do not DIY from vibes: choose a Unicode helper approach early.

Define and enforce a width policy everywhere (emoji/VS/ZWJ consistency).

Engine must provide deterministic primitives:

UTF-8 decoding (invalid sequences deterministic)

grapheme-safe operations (no half glyph)

column width measurement + wrapping

6) Code Standards (Readability + Performance)
6.1 Architectural rules

Core is platform-agnostic; platform layer implements OS interactions behind a clean interface.

Hot paths (diff, drawlist execution, output emission, input decode) must:

avoid hidden allocations

avoid per-event/per-cell malloc

be contiguous-buffer oriented

6.2 Style rules

One module per file: foo.c + foo.h with clear responsibility.

Prefer explicit names over cleverness:

engine_*, plat_*, utf8_*, fb_*, diff_*, dl_*.

Avoid macro-heavy “generic programming”. Macros allowed only for:

asserts

small constants/caps

MIN/MAX (carefully)

No global mutable state (except immutable tables).

No UB tricks:

no type-punning through pointer casts

drawlist parsing must use safe reads (memcpy for unaligned)

Consistent error model:

0 = OK, negative error codes for failures (or explicit engine_result_t enum)

asserts for internal invariants (ENGINE_ASSERT)

Comments explain why, not what.

6.3 Performance hygiene rules

Minimize syscalls: buffer output and flush once per frame.

Avoid heap churn: use arena for scratch.

Prefer stable caps + bounded queues (backpressure policy defined).

7) Allocator & Memory Ownership Model (Locked)
7.1 Core principle

The engine owns all memory it allocates. The caller never frees engine memory.

The caller provides:

input buffers (drawlist bytes, user event payload)

output buffers (event batch output, metrics output if packed)

7.2 Lifetime tiers

A) Engine lifetime (malloc/realloc)
Freed in engine_destroy:

engine state

platform state

prev/next framebuffers

persistent output buffer capacity

event ring storage

persistent caches (if any)

B) Frame lifetime (frame_arena)
Reset once per engine_present():

diff scratch (dirty lines/spans)

transient output assembly scratch (if not using persistent output buffer)

temporary measurement/wrap buffers

debug overlay scratch

C) Poll lifetime (optional poll_arena)
Reset at end of engine_poll_events() if used:

input parse scratch

escape decode scratch

event coalescing helpers

7.3 Arena requirements

Bump allocator with aligned allocations

Grows by allocating blocks as needed

arena_reset() is O(number_of_blocks) or better

arena_release() frees blocks

Hard caps to prevent runaway memory usage (configurable)

7.4 ABI safety rule

Engine must not return heap pointers requiring caller free.

Preferred ABI pattern:

engine_poll_events() writes packed events into caller-provided buffer.

engine_submit_drawlist() must not retain caller drawlist memory unless replay/diagnostics explicitly enabled (then engine copies).

8) C Core Engine — Final Required Feature List
Scope

This document defines the non-negotiable feature set and ABI contract for the C core engine.
The engine owns terminal IO, input decoding, scheduling, rendering, and diff output.
Wrappers submit drawlists and consume normalized events.

A) Platform & Terminal Control
A1. Cross-platform terminal backend

POSIX (Linux/macOS)

termios raw mode

nonblocking input

stdout writes

Windows

VT mode enabled

modern console support (Windows Terminal / ConPTY compatible)

A2. Raw mode lifecycle

Enter raw mode on engine start

Restore terminal state on exit

Alternate screen enter/leave

Cursor hide/show

Enable/disable mouse reporting modes

Enable/disable line wrap (as required)

A3. Crash & abort resiliency

Best-effort terminal restoration on:

signals (POSIX)

console close / abort paths (Windows)

Panic-safe cleanup path on engine-detected fatal errors

B) Input System (Decode + Normalize)
B1. UTF-8 decoding

Robust UTF-8 text decoding

Invalid sequences handled deterministically (no UB, no leaks)

B2. Keyboard input

Normalized key events for:

Arrows, Home/End, PageUp/PageDown

Insert/Delete, Backspace, Enter, Tab, Escape

Function keys F1–F24

Modifier normalization:

Ctrl / Alt / Shift

Consistent encoding across platforms

Strict separation:

text input (runes)

control keys

B3. Mouse input

Click, release

Drag, move

Wheel scroll

Modifier state included with mouse events

B4. Paste handling

Bracketed paste support

Paste delivered as single text-chunk event

No per-character paste emission

B5. Resize detection

Detect terminal size changes

Emit resize events (coalesced)

C) Event Loop & Scheduling
C1. Engine-owned loop

Single authoritative engine loop

Engine owns:

terminal IO

scheduling

presentation

C2. Event polling

Single poll call returns batch of events

Event types:

key

text

paste

mouse

resize

tick

user/custom

C3. Thread-safe event injection

Any thread may post user events

Engine wakes immediately (no busy wait)

C4. Timers & ticks

Built-in tick events for:

animations

spinners

debounced UI updates

C5. Backpressure & coalescing

Coalesce redundant redraw requests

Coalesce resize events (keep last)

Bounded queues:

drop or compress when producers outpace UI

C6. Frame pacing

Configurable max refresh rate

Optional adaptive pacing to avoid terminal flooding

D) Rendering Model (Framebuffer)
D1. Offscreen framebuffer

Full-terminal 2D grid of cells

Double-buffered:

previous frame

next frame

D2. Cell model

Unicode text (grapheme-safe representation)

Foreground/background color (24-bit truecolor)

Attributes (minimum):

bold

dim

italic

underline

inverse

strikethrough

D3. Clipping

Clip stack:

push rect

pop

All draw ops respect clipping

E) Unicode & Text Correctness (Engine-Owned)
E1. Display width correctness

Correct column width for:

combining marks

wide (CJK) characters

emoji / variation selectors (defined policy)

E2. Measurement primitives

Deterministic utilities:

measure text width (columns)

compute wrap positions

Tab handling:

configurable tab width

consistent expansion

E3. Grapheme-safe rendering

No half-glyph artifacts

No cursor drift due to width miscalculation

F) Diff Renderer (Minimal Terminal Output)
F1. Diff algorithm

Compare previous vs next framebuffer

Compute minimal changes

F2. Terminal state tracking

Track:

cursor position

active style state
Minimize escape sequences

F3. Output optimization

Dirty-line detection / hashing

Run-length emission (contiguous spans)

Avoid redundant cursor/style changes

F4. Single buffered flush

Build output into one buffer per frame

Single write/flush per present cycle

G) Drawlist Execution ABI
G1. Submission model

Wrapper submits one binary drawlist per frame

Versioned header

Endianness defined (little-endian)

Validated lengths and bounds

G2. String / blob tables

Drawlist includes:

string table

blob table
Commands reference offsets (no inline allocations)

G3. Required command set

CLEAR(style)

CLIP_PUSH(rect), CLIP_POP()

FILL_RECT(rect, style)

DRAW_TEXT(x, y, text_ref, style)

DRAW_TEXT_RUN(x, y, run_ref)

DRAW_HLINE(x, y, w, style)

DRAW_VLINE(x, y, h, style)

DRAW_BOX(rect, style, border_kind, title_ref?)

DRAW_GLYPH(x, y, glyph, style)

DRAW_SCROLLBAR_V(rect, top, visible, total, style)

DRAW_SCROLLBAR_H(rect, left, visible, total, style)

BLIT_RECT(src_rect, dst_pos)

G4. Safety & validation

Invalid drawlists rejected safely

Hard caps:

payload size

command count

No terminal corruption on failure

G5. Present model

submit_drawlist() updates next framebuffer

present() performs diff + output
(or combined submit+present)

H) Scrolling Acceleration
H1. Engine-level optimization

Detect large region shifts

Use:

framebuffer blits

terminal-native scroll commands (when reliable)

H2. Correctness guarantees

No visual corruption

Safe fallback to redraw

H3. Partial invalidation (optional)

Dirty region marking API for wrappers

I) Capability Detection & Fallbacks
I1. Terminal probing

Probe:

Color support: truecolor / 256-color / basic

Mouse reporting support

Unicode assumptions (practical baseline)

I2. Graceful degradation

Truecolor → nearest palette

Unsupported attributes degrade predictably

J) Diagnostics & Debugging
J1. Performance counters

Time spent in:

input decode

drawlist parse/execute

diff computation

output write/flush

J2. Frame statistics

bytes written

dirty cells/lines

frames presented/dropped

event queue depth/drops

J3. Debug overlay

Optional overlay:

FPS

perf stats

dirty regions

J4. Logging hooks

Engine log callback

Severity + message

J5. Deterministic replay

Record event batches

Replay for debugging

Engine-defined format

K) Configuration & ABI
K1. Runtime configuration

Target FPS / max refresh

Tab width

Mouse enable/disable

Alt screen enable/disable

Color mode preference

Scroll optimization toggle

Diagnostics toggle

K2. ABI versioning

Engine ↔ wrapper ABI negotiation

Feature flags

Drawlist format versioned independently

Forward-compatible policy

L) Minimal C ABI Surface (Locked)
engine_create(config) -> handle
engine_destroy(handle)

engine_poll_events(handle, timeout_ms, out_event_buffer, out_len) -> count_or_bytes

engine_post_user_event(handle, tag, payload_ptr, payload_len) (thread-safe)

engine_submit_drawlist(handle, bytes_ptr, bytes_len) -> ok/error

engine_present(handle) -> ok/error

engine_get_metrics(handle, out_metrics)

engine_set_config(handle, key_value_or_struct)

Appendix: Implementation Guardrails (Must Follow)

No per-frame heap allocations in hot paths (use arena).

No platform #ifdef in core.

All failures must leave terminal in a recoverable state when possible.

Deterministic behavior is preferred: same events + same drawlists produce consistent output under same terminal capability flags.

Hard caps + bounded queues are required for safety.

9) Unit Testing & Verification Strategy (Locked)
9.1 Goals

Testing must validate:

correctness of UTF-8 decode, grapheme handling, width measurement, wrapping

drawlist parsing/validation and framebuffer execution correctness

diff renderer correctness + output minimality invariants

event parsing/normalization correctness (keyboard/mouse/paste/resize)

backpressure/coalescing behavior deterministically

arena allocator safety + caps behavior

platform terminal control correctness via integration tests, not unit tests

Priority: deterministic, automatable tests that run in CI without “real terminal” flakiness.

9.2 Test Types (what we test where)
A) Pure unit tests (fast, deterministic, no OS)

Run on all platforms and toolchains.

Targets:

/util: arena, vec, ring, string_view, string_builder

/unicode: utf8 decode, width, grapheme segmentation (as implemented), wrapping

/core: drawlist validation + framebuffer execution + diff algorithm (in-memory)

These tests must not depend on terminal IO or platform layer.

B) Golden tests (byte-for-byte output)

For:

diff renderer output bytes given:

previous framebuffer state

next framebuffer state

starting terminal state (cursor/style assumptions)

capability flags (truecolor/256/basic)

Golden outputs are stored as files and compared exactly.
This locks behavior and prevents regressions.

C) Property tests / fuzz tests (must-have for parsers)

At minimum:

drawlist parser fuzz target

escape sequence / input parser fuzz target (if present)

UTF-8 decoder fuzz target

Fuzzing should assert:

no crashes, no hangs

no OOM beyond caps

invalid input returns deterministic error codes

terminal state not corrupted (internal invariants hold)

D) Integration tests (platform + terminal)

These are allowed to be fewer and slower.
They validate:

raw mode lifecycle restoration

VT enable on Windows

basic IO loop works

resize events generated

Rule: integration tests must not require “human” terminals. They must run headless where possible (ConPTY on Windows, pseudo terminals on POSIX).

9.3 Test Harness / Framework Choice (C)

Use a small single-file C test framework to avoid dependency hell. Options:

a tiny custom harness (recommended)

or lightweight libs (e.g., cmocka, Unity) if desired

Locked requirement: tests must be runnable via ctest from CMake.

Minimum harness features:

TEST(name) macro registration

ASSERT_EQ, ASSERT_TRUE, ASSERT_MEMEQ

per-test isolation (no shared global state)

deterministic seed printing for any randomized tests

9.4 Determinism Requirements (for stable CI)

No wall-clock dependency in unit tests (use injected clock where needed).

No reliance on current locale; enforce UTF-8 behavior explicitly.

No flaky timing waits; any polling uses deterministic simulated inputs.

Golden tests must pin:

capability flags

initial cursor/style assumptions

width policy mode

9.5 Core Testability Design Rules (must design for tests)

To enable unit testing:

Core modules must accept in-memory dependencies:

platform interface mocked in tests

write sink can be a memory buffer sink

time source can be injected (or overridden in tests)

Diff renderer must be callable as a pure function:

(prev_fb, next_fb, caps, out_buf) -> bytes_written

Drawlist execution must be testable without terminal:

(fb, drawlist_bytes) -> result

9.6 Suggested Test Coverage Map (minimum bar)
/util

arena:

alignment correctness

grow behavior

reset behavior

caps enforcement

no overwrite across allocations

vec/ring:

push/pop/resize semantics

wrap behavior for ring

bounded drops policy correctness

string_builder:

append correctness

growth/caps behavior

/unicode

UTF-8 decode:

valid sequences

invalid sequences deterministic replacement/error policy

width:

combining marks

wide CJK

emoji sequences per chosen policy

wrapping:

exact wrap positions

tab expansion rules

/core

drawlist validator:

bounds checks

table offsets validity

command count caps

rejects invalid lists without side effects

drawlist execution:

clipping behavior

fill/text/box lines correctness

blit correctness

diff renderer:

minimality invariants (no redundant cursor/style changes)

dirty line detection correctness

single flush buffer semantics

9.7 CI & Sanitizers (locked expectations)

CI must run:

unit tests on all OS/toolchains

golden tests on at least one per OS (preferably all)

sanitizers where supported:

ASan + UBSan on Linux/macOS (clang)

on Windows: enable clang-cl sanitizers if feasible; otherwise rely on runtime checks + fuzz in nightly

Also:

build with warnings-as-errors in CI for core/utf/unicode/util (platform may require selective pragmas).

9.8 Testing Terminal / Platform Edge Cases

Terminal edge cases are validated via integration tests:

POSIX: use PTY-based tests to simulate terminal I/O and verify restoration.

Windows: use ConPTY where possible; verify VT enable and input decoding.

Where terminal emulation differs, tests assert:

engine’s chosen capability-driven behavior matches expectation

degradation is predictable

Rule: unit tests must not attempt to “drive a real human terminal”.

9.9 Output Golden File Format

Golden outputs should be stored as:

raw byte dumps (binary) or

escaped text representation (hex + printable) for easier diff review

Each golden test fixture specifies:

capabilities (truecolor vs 256)

width policy mode

initial terminal style state assumptions

expected emitted byte stream

9.10 Definition of Done for Changes

A change is not complete unless:

unit tests pass

golden outputs are unchanged or intentionally updated with rationale

fuzz targets still run clean (no crash regressions)

sanitizer runs (where configured) are clean

9.11 Minimal CMake Requirements for Tests

ENABLE_TESTING()

add_test(...) targets for:

unit

golden

integration (optional in PR CI, required in nightly)

Make it easy for CODEX/devs:

cmake --build --target test

ctest --output-on-failure

End of Unit Testing Strategy (Locked).

End of Master Spec (Locked).