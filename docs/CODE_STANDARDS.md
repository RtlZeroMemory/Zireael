# Code Standards (Normative)

## Purpose and scope (what it owns / what it does NOT own)

This document defines Zireael’s coding standards that are not purely “safety rules”:

- naming conventions and module structure
- documentation/comment requirements
- assert usage policy
- macro/global-state policy

It complements (and must not contradict):

- `docs/SAFETY_RULESET.md` (LOCKED safety + determinism + concurrency rules)
- `docs/LIBC_POLICY.md` (LOCKED libc policy)

## Readability and structure

These rules exist to keep the engine **boring, readable, and deterministic**. Favor obvious code over clever code.

This document uses **MUST / MUST NOT / SHOULD** language on purpose.

### Readability first

- Code MUST optimize for maintainability by humans and future LLM contributors.
- Micro-optimizations MUST NOT be accepted if they materially harm clarity.
- Dense expressions SHOULD be split into named intermediate variables.

### Function size and shape

- Functions SHOULD be **20–40 lines** and MUST generally be **< 50 lines**.
- A function MUST do one thing. If you need comments to explain the structure of the function, it is too big.
- Prefer “parse/validate/execute” style decomposition over multi-purpose helpers.

### Comments policy (Why > What)

- Comments MUST explain:
    - why something is done
    - what invariant is being protected
    - what assumption the code relies on (e.g., “validated view”)
- Comments MUST NOT restate code line-by-line.
- Tricky parsing/framing logic SHOULD be annotated with brief “shape” comments (header → ranges → spans → stream).

#### Comment style (format and tone)

- Prefer **short, structured block comments** for non-trivial reasoning (typically 5–20 lines).
- Use `/* ... */` for single-line comments and multi-line blocks; avoid `//` in engine code.
- Multi-line blocks SHOULD follow the aligned `*` style when the comment spans more than ~2 lines:

```c
/*
 * What this block does (high-level).
 *
 * Why it exists (invariant, deterministic policy, or subtle edge case).
 * Any important caller/callee contract notes.
 */
```

- Comments SHOULD be factual and specific; avoid generic filler (no “AI-sounding” commentary).

#### Strategic comments (where to add them)

Add comments only where they carry real information that is not obvious from the code:

- **Algorithm shape and invariants** (e.g., ring buffer head/tail layouts, wrap-around behavior).
- **Deterministic policy notes** (e.g., invalid UTF-8 replacement behavior, tie-break rules).
- **State machines** (e.g., grapheme boundary rule state tracking like GB11).
- **Long functions**: add short section markers when a function has multiple conceptual phases (reset → attrs → colors →
  finalize).
- **Defensive NULL checks**: only when non-obvious (e.g., "cleanup path accepts NULL", "lock is no-op on NULL for error
  paths").

#### Function-level comments (when required)

Non-trivial functions MUST have a brief comment at the top explaining **what** the function does (not how). This applies
to:

- **Public API functions** (`zr_*` that appear in headers and are called by other modules or callers).
- **Functions > 20 lines** that do something non-obvious from the name alone.
- **Functions with subtle behavior** (e.g., coalescing, tie-breaking, capability downgrading).
- **Internal helpers** that are called from multiple places or have non-obvious contracts.

Function-level comments SHOULD be 1–3 lines and answer: "What does this function do and why would I call it?"

Example:

```c
/* Map 24-bit RGB to nearest xterm 256-color index, comparing both
 * the 6x6x6 color cube and grayscale ramp to find the best match. */
static uint8_t zr_rgb_to_xterm256(uint32_t rgb) {
```

Functions that do NOT need function-level comments:

- Trivial one-liners (e.g., `zr_rgb_r()`, `zr_style_default()`).
- Simple getters/setters where the name is self-explanatory.
- Static helpers under ~10 lines with obvious intent from the name.

### File header comments (required)

Every `.c`/`.h` file MUST start with a brief “what/why” header.

Example:

```c
/*
  src/core/zr_drawlist.c — Drawlist validator and executor.

  Why: Validates wrapper-provided bytes and executes drawing into the framebuffer safely.
  Invariants:
    - No OOB reads; all offsets/sizes validated before access.
    - No partial effects: validate fully before mutating the framebuffer.
  Failure modes:
    - Invalid format -> ZR_ERR_FORMAT / ZR_ERR_UNSUPPORTED.
    - Caps exceeded -> ZR_ERR_LIMIT.
*/
```

## Public API (function signatures and types; include header file names)

Public-facing code conventions (applies repo-wide):

- filename conventions, header layout, include discipline
- stable prefixes for public and internal symbols

## Invariants (explicit MUST-always-hold statements)

### Naming (normative)

Function prefixes MUST be explicit and stable:

- `engine_*` — public ABI and orchestration
- `dl_*` — drawlist parsing/validation/execution
- `fb_*` — framebuffer operations
- `diff_*` — diff renderer
- `utf8_*` / `grapheme_*` / `width_*` / `wrap_*` — Unicode primitives
- `plat_*` — platform interface and backends
- `zr_*` — shared base types/utilities

Names MUST describe intent:

- Functions: `arena_alloc_aligned()`, `dl_validate_header_v1()`, `fb_cell_index()`
- Variables: `write_offset`, `bytes_remaining`, `span_index_bytes`

Avoid one-letter names except for trivial loops (`i`, `j`) or small, obvious scopes.

Type naming:

- typedef’d structs/enums MUST use `*_t` suffix.
- ABI-visible types MUST use fixed-width integers (`uint32_t`, `int32_t`, etc.).
- Public ABI surfaces MUST keep stable names and signatures; refactors MUST NOT rename public API.

### Module structure (normative)

- Code MUST be split into modules with single responsibility (`foo.h` + `foo.c`).
- Headers MUST minimize includes and MUST NOT leak platform headers across boundaries.
- OS types MUST NOT cross the platform boundary (use POD + opaque handles).

### Ownership, lifetimes, and out-params (normative)

- Every struct that contains pointers MUST document:
    - who owns the pointed-to memory
    - how long it must remain valid
    - whether the pointer can be NULL
- The engine MUST NOT return heap pointers that require the caller to `free()`.
- Out-parameters MUST be either:
    - fully written on success and left in a documented safe state on failure, or
    - explicitly zeroed at the start of the function (before any early returns).

### Pointer arithmetic and bounds checks (normative)

- Pointer arithmetic MUST be written in steps with checked math:
    - compute offsets with `zr_checked_*`
    - validate ranges before creating derived pointers
- Prefer `if (!ptr)` / `if (ptr)` for NULL checks in simple conditions; keep explicit `== NULL` / `!= NULL` only when it
  materially improves readability in complex boolean expressions.
- Avoid “clever” casts. Unaligned reads MUST use byte loads or `memcpy` helpers (no type-punning).
- Any non-obvious boundary check MUST have a brief comment explaining the invariant being protected.

### Macro policy (normative)

- Macro-heavy “generic programming” is discouraged.
- Macros are allowed only for:
    - assertions
    - small constants/caps
    - carefully-audited `ZR_MIN`/`ZR_MAX` (if used)
- Macros MUST NOT hide allocations or complex control flow.

### Global state policy (normative)

- Global mutable state is FORBIDDEN (except immutable tables in Unicode/data).

### Assertions (normative)

- Asserts are for internal invariants and programmer errors only.
- Asserts MUST NOT be used to validate untrusted input (drawlists, platform input bytes).

## Failure modes & error codes

Coding standard violations are treated as bugs and are handled via:

- compile failures (warnings-as-errors in CI where feasible)
- test failures
- debug assertions

| Condition                                       | Required behavior                                      | Return code |
|-------------------------------------------------|--------------------------------------------------------|------------:|
| Untrusted input validation done via assert      | Treat as bug; refactor to bounds-checked error returns |         n/a |
| Module includes OS headers in core/unicode/util | Treat as bug; CI guardrail should fail                 |         n/a |

## Error handling and “No Partial Effects”

- Success is `ZR_OK == 0`. Failures are negative `ZR_ERR_*` codes.
- Failure paths MUST be obvious:
    - use early returns for simple functions
    - use `goto cleanup` for multi-resource functions
- Code MUST NOT have hidden side effects on failure:
    - validate fully before mutating state when possible
    - do not partially write output buffers unless the API explicitly allows it

Example cleanup pattern:

```c
zr_result_t rc = ZR_OK;
resource_t* r = NULL;

r = alloc_resource();
if (!r) { rc = ZR_ERR_OOM; goto cleanup; }

cleanup:
free_resource(r);
return rc;
```

## Performance notes (hot paths, allocation rules, complexity targets)

- Keep hot path code simple and auditable:
    - small focused functions
    - minimal branching where possible
    - avoid hidden allocations
- Prefer contiguous buffers and explicit bounds checks in parsers.

## Concurrency rules (what can be called from which thread; locking)

Concurrency rules are locked in `docs/SAFETY_RULESET.md`.

Coding standards requirement:

- Thread-affinity and thread-safety requirements MUST be documented in each module header for any public/engine
  entrypoints.

## Deterministic behavior guarantees

Determinism requirements are locked in `docs/SAFETY_RULESET.md`.

Coding standards requirement:

- Any behavior change that affects:
    - binary formats
    - output byte streams
    - event ordering/coalescing
      must update the corresponding module doc and tests/fixtures in the same change.

## Test plan

- Unit tests and fuzz targets must cover parsers and invariants.
- Golden tests must pin output byte streams and prevent regressions.

See `docs/modules/TESTING_GOLDENS_FUZZ_INTEGRATION.md`.
