# [P1] Explicit Non-TTY/Pipe Output Mode

- Labels: `hardening`, `platform`, `posix`, `io-policy`, `abi`
- Source: `docs/audit/missing-capabilities.md` (MC-002)

## Problem
When stdio is detached/non-TTY, POSIX backend currently rebinds to `/dev/tty` instead of exposing an explicit policy/mode for non-TTY output.

## Acceptance Criteria
- Introduce explicit config policy for non-TTY stdio handling (`fail`/`tty-fallback`/`pipe-mode`).
- Implement and document behavior for each policy.
- Preserve existing default behavior for backward compatibility unless opt-in is provided.
- Add integration tests for detached stdio scenarios covering all policy modes.
