# Zireael — Code Standards (Readability + Safety)

These rules exist to keep the engine **boring, readable, and deterministic**. Favor obvious code over clever code.

This document uses **MUST / MUST NOT / SHOULD** language on purpose.

## 1) Readability First

- Code MUST optimize for maintainability by humans and future LLM contributors.
- Micro-optimizations MUST NOT be accepted if they materially harm clarity.
- Dense expressions SHOULD be split into named intermediate variables.

## 2) Function Size and Shape

- Functions SHOULD be **20–40 lines** and MUST generally be **< 50 lines**.
- A function MUST do one thing. If you need comments to explain the structure of the function, it is too big.
- Prefer “parse/validate/execute” style decomposition over multi-purpose helpers.

## 3) Naming

- Names MUST describe intent:
  - Functions: `arena_alloc_aligned()`, `dl_validate_header_v1()`, `fb_cell_index()`
  - Variables: `write_offset`, `bytes_remaining`, `span_index_bytes`
- Avoid one-letter names except for trivial loops (`i`, `j`) or small, obvious scopes.
- Public ABI surfaces MUST keep stable names and signatures; refactors MUST NOT rename public API.

## 4) Ownership, Lifetimes, and Out-Params

- Every struct that contains pointers MUST document:
  - who owns the pointed-to memory
  - how long it must remain valid
  - whether the pointer can be NULL
- The engine MUST NOT return heap pointers that require the caller to `free()`.
- Out-parameters MUST be either:
  - fully written on success and left in a documented safe state on failure, or
  - explicitly zeroed at the start of the function (before any early returns).

## 5) Pointer Arithmetic and Bounds Checks

- Pointer arithmetic MUST be written in steps with checked math:
  - compute offsets with `zr_checked_*`
  - validate ranges before creating derived pointers
- Avoid “clever” casts. Unaligned reads MUST use byte loads or `memcpy` helpers (no type-punning).
- Any non-obvious boundary check MUST have a brief comment explaining the invariant being protected.

## 6) Error Handling and “No Partial Effects”

- Success is `ZR_OK == 0`. Failures are negative `ZR_ERR_*` codes.
- Failure paths MUST be obvious:
  - use early returns for simple functions
  - use `goto cleanup` for multi-resource functions
- Code MUST NOT have hidden side effects on failure:
  - validate fully before mutating state when possible
  - do not partially write output buffers unless the API explicitly allows it

## 7) Macros

- Macros MUST be small and local.
- Macros MUST NOT hide control flow or perform non-trivial memory manipulation.
- If a macro is required (e.g., test auto-registration), it MUST be documented with:
  - why a function cannot be used instead
  - which compilers/platforms it targets

## 8) Comments Policy (Why > What)

- Comments MUST explain:
  - why something is done
  - what invariant is being protected
  - what assumption the code relies on (e.g., “validated view”)
- Comments MUST NOT restate code line-by-line.
- Tricky parsing/framing logic SHOULD be annotated with brief “shape” comments (header → ranges → spans → stream).

## 9) Platform Boundary (Hard Rule)

- Files under `src/core/`, `src/unicode/`, and `src/util/` MUST NOT include OS headers.
- OS-specific code MUST live under `src/platform/posix/` and `src/platform/win32/`.
- `#ifdef _WIN32` MUST be confined to platform backends and minimal backend-selection glue.

## 10) Determinism

- Tests and core logic MUST be deterministic:
  - no locale-dependent behavior
  - no wall-clock dependencies in unit tests
  - no random seeds unless they are fixed and documented

