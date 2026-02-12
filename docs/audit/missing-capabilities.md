# Missing Capabilities / Not Yet Implemented

This report captures capabilities still missing after the C engine hardening pass. Items are evidence-based and derived from audited code paths.

## Summary

| Metric | Count |
|---|---:|
| Total missing capabilities | 6 |
| P0 | 1 |
| P1 | 3 |
| P2 | 2 |
| IN-SCOPE | 2 |
| OUT-OF-SCOPE | 4 |

## MC-001: Crash-Safe Raw-Mode Restore Hooks
- Why it matters: If the process exits abnormally while raw mode/alt-screen is active, the terminal can remain in a degraded state for the user shell.
- Evidence: `src/platform/posix/zr_plat_posix.c:1080` and `src/platform/posix/zr_plat_posix.c:1158` show restore happens in `plat_destroy`/`plat_leave_raw`; search term `SIGTERM|SIGINT|SIGABRT|atexit` in `src/platform/posix/zr_plat_posix.c` has no restore hook.
- Current observed behavior: Restore is best-effort on normal teardown only.
- Desired behavior: Best-effort terminal restore also runs on common abnormal process terminations.
- Proposed implementation direction: Add optional process-wide restore hook registration (signal handlers + safe restoration path), with strict chaining and idempotence.
- Suggested tests to add: Integration test that enters raw mode in a child process, triggers termination signal, and verifies cooked mode is restored in parent PTY.
- Priority: P0
- In-scope for this PR: OUT-OF-SCOPE (process-global signal policy and host integration behavior).

## MC-002: Explicit Non-TTY/Pipe Output Mode
- Why it matters: Robust terminal engines need predictable behavior when stdio is detached or piped (CI, wrappers, redirection scenarios).
- Evidence: `src/platform/posix/zr_plat_posix.c:988` to `src/platform/posix/zr_plat_posix.c:1009` forcibly rebinds to `/dev/tty` when stdin/stdout are not both TTY.
- Current observed behavior: Engine falls back to controlling TTY instead of offering a defined non-TTY output mode.
- Desired behavior: Configurable policy: fail-fast, TTY fallback, or explicit non-TTY output mode.
- Proposed implementation direction: Add platform config policy flag and a non-raw pipe mode path that skips termios-only operations.
- Suggested tests to add: Integration tests for detached stdio with each policy mode (`fail`, `fallback`, `pipe`).
- Priority: P1
- In-scope for this PR: OUT-OF-SCOPE (new runtime behavior and config surface).

## MC-003: POSIX SIGPIPE-Safe Output Contract
- Why it matters: When writing to disconnected pipes, the process should return a controlled engine/platform error instead of risking process termination.
- Evidence: `src/platform/posix/zr_plat_posix.c:819` to `src/platform/posix/zr_plat_posix.c:854` handles `EINTR/EAGAIN` in write loop; search term `SIGPIPE` across `src/` has no explicit handling.
- Current observed behavior: No explicit project-level SIGPIPE policy is enforced by the backend.
- Desired behavior: Guaranteed conversion of broken-pipe writes to `ZR_ERR_PLATFORM` without process-level termination.
- Proposed implementation direction: Introduce documented POSIX SIGPIPE policy (ignore or scoped mitigation) with compatibility notes.
- Suggested tests to add: Integration test where output endpoint closes mid-stream and engine returns error deterministically.
- Priority: P1
- In-scope for this PR: OUT-OF-SCOPE (process-global signal handling policy).

## MC-004: Win32 Output-Writability Wait Parity
- Why it matters: Backpressure pacing should have consistent semantics across Win32 console/ConPTY environments.
- Evidence: `src/platform/win32/zr_plat_win32.c:666` to `src/platform/win32/zr_plat_win32.c:676` enables output-writable wait capability only for `FILE_TYPE_PIPE`; `src/platform/win32/zr_plat_win32.c:1238` returns `ZR_ERR_UNSUPPORTED` otherwise.
- Current observed behavior: `plat_wait_output_writable` is unavailable on many non-pipe output handles.
- Desired behavior: Equivalent output pacing support across supported Win32 output handle types.
- Proposed implementation direction: Implement a console-compatible pacing strategy and expose consistent capability behavior.
- Suggested tests to add: Win32 integration matrix tests for ConPTY vs console handles under output pressure.
- Priority: P2
- In-scope for this PR: OUT-OF-SCOPE (platform feature expansion with host-specific behavior).

## MC-005: Dedicated Diff Renderer LibFuzzer Harness
- Why it matters: Diff emission is a high-complexity hot path; dedicated coverage-guided fuzzing catches state-machine edge cases beyond deterministic fixtures.
- Evidence: `tests/CMakeLists.txt:298` to `tests/CMakeLists.txt:301` defines libFuzzer targets for drawlist/input/utf8/grapheme only; no diff-renderer libFuzzer target is registered.
- Current observed behavior: Diff renderer is covered by unit+golden tests, but not by a dedicated libFuzzer harness.
- Desired behavior: Add a coverage-guided fuzz target for `zr_diff_render_ex` with constrained framebuffer/state generation.
- Proposed implementation direction: Add `tests/fuzz/libfuzzer_diff_renderer.c` and register it under `ZIREAEL_BUILD_LIBFUZZER`.
- Suggested tests to add: New libFuzzer target with CI smoke run and nightly extended run budgets.
- Priority: P1
- In-scope for this PR: IN-SCOPE

## MC-006: Dedicated Event-Pack LibFuzzer Harness
- Why it matters: Event batch framing/truncation is ABI-facing and safety-critical; direct fuzzing strengthens guarantees around header/record invariants.
- Evidence: `tests/fuzz/zr_fuzz_smoke.c:119` to `tests/fuzz/zr_fuzz_smoke.c:183` exercises event packer in smoke mode; `tests/CMakeLists.txt:298` to `tests/CMakeLists.txt:301` has no dedicated libFuzzer target for event packer.
- Current observed behavior: Event packer gets indirect smoke fuzz coverage only.
- Desired behavior: Dedicated libFuzzer target for `zr_evpack_begin/append/finish` invariants and truncation behavior.
- Proposed implementation direction: Add `tests/fuzz/libfuzzer_event_pack.c` and include deterministic invariant checks (no partial record bytes, header integrity).
- Suggested tests to add: libFuzzer CI smoke + nightly budget and corpus seeding from known edge cases.
- Priority: P2
- In-scope for this PR: IN-SCOPE

## Out-of-Scope Issue Stubs

- `docs/audit/issues/001-crash-safe-restore-hooks.md`
- `docs/audit/issues/002-explicit-non-tty-pipe-mode.md`
- `docs/audit/issues/003-posix-sigpipe-safe-output-policy.md`
- `docs/audit/issues/004-win32-output-writability-parity.md`
