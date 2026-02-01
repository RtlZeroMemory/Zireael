---
name: zireael-error-contracts
description: Keep `ZR_ERR_*` return codes and “no partial effects” contracts consistent across Zireael.
metadata:
  short-description: Error codes + state effects
---

## When to use

Use this skill when:

- adding/modifying public `engine_*` APIs
- writing parsers/validators (drawlist, input, UTF-8)
- changing cap enforcement (`zr_limits_t`)
- changing “no partial effects” behavior
- adding tests around failures/recovery

## Source of truth (locked)

- `docs/ERROR_CODES_CATALOG.md` (single source for `ZR_ERR_*` semantics + state effects)
- `docs/SAFETY_RULESET.md` (Safe C + determinism + cleanup rules)
- Module failure-mode tables under `docs/modules/**`

## Core rules (must follow)

- `ZR_OK == 0`
- failures are negative (`ZR_ERR_*`)
- default: **no partial effects** on failure
- the only permitted “partial output” mode in v1 is **event batch truncation**, which is a **successful** return with `TRUNCATED` set in the batch header (not a negative error)

## Implementation checklist (before writing code)

1. Identify the failure classes your code can hit:
   - invalid args → `ZR_ERR_INVALID_ARGUMENT`
   - caps/output buffer too small → `ZR_ERR_LIMIT`
   - malformed bytes/format → `ZR_ERR_FORMAT`
   - unsupported version/opcode/feature → `ZR_ERR_UNSUPPORTED`
   - alloc failure (heap/arena growth) → `ZR_ERR_OOM`
   - backend failure → `ZR_ERR_PLATFORM`
2. Decide “state mutated?” explicitly using the API-level contracts in `docs/ERROR_CODES_CATALOG.md`.
3. Ensure validators run fully before any mutation (drawlist especially).
4. Ensure writers never emit partial records (events) or partial flushes (present/diff).

## Review checklist (before finalizing)

- Every public API documents its negative return codes.
- Errors do not silently succeed or partially mutate caller-observable state.
- `engine_present` never flushes partial output on `ZR_ERR_LIMIT`/`ZR_ERR_INTERNAL`.
- Parsers:
  - never read OOB
  - return deterministic codes for malformed inputs
  - make progress (no infinite loops)

## Test guidance (what to add)

- Unit tests for invalid args return `ZR_ERR_INVALID_ARGUMENT` and do not mutate state.
- Cap tests return `ZR_ERR_LIMIT` and do not mutate state.
- Parser tests confirm `ZR_ERR_FORMAT`/`ZR_ERR_UNSUPPORTED` on bad inputs with no partial effects.
- Event packing tests confirm truncation is a **success** result with `TRUNCATED` flag set and only complete records written.

