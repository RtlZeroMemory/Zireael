---
name: zireael-code-style
description: Write readable C with proper comments, named constants, and consistent style per CODE_STANDARDS.md.
metadata:
  short-description: Code style + comments + constants
---

## When to use

Use this skill when:

- writing new C code
- reviewing code for style compliance
- adding comments to existing code
- extracting magic numbers to constants
- normalizing coding patterns

## Source of truth

- `docs/CODE_STANDARDS.md` — naming, comments, function size, ownership docs
- `docs/SAFETY_RULESET.md` — safety and determinism rules
- `docs/LIBC_POLICY.md` — allowed/forbidden libc functions

## File header comments (required)

Every `.c`/`.h` file MUST start with:

```c
/*
  path/to/file.c — Brief description (one line).

  Why: Explain the purpose and key design decisions.
*/
```

## Function-level comments (required for non-trivial functions)

**MUST have a function-level comment when:**

- Public API function (appears in header, called by other modules)
- Function is > 20 lines with non-obvious behavior
- Function has subtle behavior (coalescing, tie-breaking, capability downgrading)
- Internal helper called from multiple places

**Function comment format (1-3 lines):**

```c
/* Map 24-bit RGB to nearest xterm 256-color index, comparing both
 * the 6x6x6 color cube and grayscale ramp to find the best match. */
static uint8_t zr_rgb_to_xterm256(uint32_t rgb) {
```

**DO NOT need function-level comment:**

- Trivial one-liners (e.g., `zr_rgb_r()`, `zr_style_default()`)
- Simple getters/setters with self-explanatory names
- Static helpers < 10 lines with obvious intent

### Inside-function comments (strategic)

Add comments inside functions for:

- Complex algorithms (brief overview, not line-by-line)
- State machines (what each state variable tracks)
- Spec-derived logic (reference the spec, e.g., "UAX #29 GB11")
- Non-obvious defensive patterns (e.g., "accepts NULL for cleanup convenience")

Do NOT add comments that:

- Restate the code (`/* increment i */ i++`)
- Explain obvious stdlib usage
- Pad length without adding value

## Strategic comment types

### ASCII diagrams for data structures

```c
/*
 * Ring buffer layout:
 *
 *   Case 1: tail >= head
 *     [....head=====tail....]
 *
 *   Case 2: tail < head (wrapped)
 *     [====tail......head====]
 */
```

### Section markers in long functions

```c
/* --- Validate inputs --- */
...
/* --- Process data --- */
...
/* --- Write outputs --- */
```

### Format reminders for bit layouts

```c
/* RGB stored as 0x00RRGGBB */
```

### Invariant protection

```c
/* Ensures offset + size doesn't overflow before pointer creation */
```

## Named constants (required)

Extract magic numbers to named constants:

```c
/* BAD */
if (align > 4096u) return false;

/* GOOD */
#define ZR_ARENA_MAX_ALIGN 4096u
if (align > ZR_ARENA_MAX_ALIGN) return false;
```

Group related constants with section comments:

```c
/* --- SGR (Select Graphic Rendition) codes --- */
#define ZR_SGR_RESET     0u
#define ZR_SGR_BOLD      1u
#define ZR_SGR_ITALIC    3u
```

## NULL check style

Use `!ptr` consistently:

```c
/* GOOD */
if (!ptr) return ZR_ERR_INVALID_ARGUMENT;

/* AVOID */
if (ptr == NULL) return ZR_ERR_INVALID_ARGUMENT;
```

## Function size

- Target: 20-40 lines
- Maximum: 50 lines
- If larger: split into helpers with clear names

## Naming conventions

| Type      | Convention               | Example                 |
|-----------|--------------------------|-------------------------|
| Functions | `module_action_noun()`   | `arena_alloc_aligned()` |
| Variables | `snake_case` descriptive | `bytes_remaining`       |
| Constants | `ZR_MODULE_NAME`         | `ZR_DL_MAGIC`           |
| Types     | `zr_name_t`              | `zr_arena_t`            |

## Review checklist

Before finalizing code:

- [ ] File has header comment with "Why"
- [ ] Non-trivial functions (> 20 lines or subtle behavior) have function-level comment
- [ ] Public API functions have function-level comment
- [ ] Complex logic inside functions has strategic comments (diagrams, state tracking, invariants)
- [ ] Magic numbers extracted to named constants
- [ ] NULL checks use `!ptr` style
- [ ] Functions under 50 lines
- [ ] No comments that restate code
