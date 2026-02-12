# [P0] Crash-Safe Raw-Mode Restore Hooks

- Labels: `hardening`, `platform`, `posix`, `win32`, `terminal-safety`
- Source: `docs/audit/missing-capabilities.md` (MC-001)

## Problem
The engine restores raw/alt-screen state on normal teardown, but does not expose a crash/abnormal-termination restore path.

## Acceptance Criteria
- Add a documented crash-safety restore design for supported platforms.
- Implement best-effort abnormal-termination restore hooks with explicit chaining policy.
- Ensure restore path is idempotent and does not introduce UB in signal contexts.
- Add integration coverage proving terminal mode restoration after child-process abnormal termination.
