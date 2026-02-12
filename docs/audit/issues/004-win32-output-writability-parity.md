# [P2] Win32 Output-Writability Wait Parity

- Labels: `hardening`, `platform`, `win32`, `conpty`, `backpressure`
- Source: `docs/audit/missing-capabilities.md` (MC-004)

## Problem
`plat_wait_output_writable` capability is effectively pipe-specific and returns unsupported on other output handle types.

## Acceptance Criteria
- Define target behavior for output pacing across Win32 output handle classes.
- Implement consistent capability semantics for supported handle types.
- Preserve explicit `ZR_ERR_UNSUPPORTED` for truly unsupported environments.
- Add Win32 integration tests covering ConPTY and native console handles.
