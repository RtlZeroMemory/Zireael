# [P1] POSIX SIGPIPE-Safe Output Policy

- Labels: `hardening`, `platform`, `posix`, `io-safety`
- Source: `docs/audit/missing-capabilities.md` (MC-003)

## Problem
No explicit project-level SIGPIPE handling policy is defined for POSIX output writes.

## Acceptance Criteria
- Define and document SIGPIPE policy for backend writes.
- Ensure broken-pipe conditions return deterministic engine/platform error rather than process termination.
- Validate behavior with integration test that closes output endpoint mid-stream.
- Confirm no regression for normal TTY output path.
