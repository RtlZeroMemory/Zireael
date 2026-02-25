---
name: zireael-code-review
description: Review C code for compliance with Zireael standards. Use before committing or when reviewing PRs.
user-invocable: true
allowed-tools: Read, Glob, Grep, Bash(cmake *), Bash(ctest *), Bash(bash scripts/*)
argument-hint: "[file-path] or [module]"
metadata:
  short-description: Code review + compliance check
---

## Response Format (IMPORTANT)

1. **Start with a 3-sentence summary** of what the code does and overall quality
2. **List specific issues with file:line references** (e.g., `src/core/engine.c:42`)
3. **Provide copyable fix suggestions** for each issue
4. **Keep total response under 500 lines**

Get to actionable output quickly. No lengthy preambles.

## When to use

Use this skill when:

- Reviewing code before commit or PR
- Checking compliance with Zireael standards
- Auditing code for safety and style issues

## Automated Checks

First run automated checks:

```bash
# Build (must pass)
cmake --preset posix-clang-debug && cmake --build --preset posix-clang-debug

# Guardrails (must pass)
bash scripts/guardrails.sh

# Tests (must pass)
ctest --test-dir out/build/posix-clang-debug --output-on-failure
```

## Platform Boundary Review

If code touches `src/core/`, `src/unicode/`, or `src/util/`:

- [ ] No OS headers (`<windows.h>`, `<unistd.h>`, `<sys/*.h>`)
- [ ] No `#ifdef _WIN32` or `#ifdef __linux__`
- [ ] No forbidden libc calls (see `docs/LIBC_POLICY.md`)
- [ ] Only portable types (`uint32_t`, `size_t`, etc.)

## Safety Review

Per `docs/SAFETY_RULESET.md`:

- [ ] No undefined behavior (type punning, unaligned access)
- [ ] Checked arithmetic for size/offset calculations
- [ ] Bounds validation before buffer access
- [ ] No partial effects (validate fully before mutating)
- [ ] Proper ownership (engine owns allocations, caller provides buffers)

## Code Style Review

Per `docs/CODE_STANDARDS.md`:

- [ ] File has header comment with "Why"
- [ ] Non-trivial functions have function-level comments
- [ ] Complex comments explain decision rationale/tradeoffs, not field glossaries
- [ ] Functions under 50 lines (target 20-40)
- [ ] Magic numbers extracted to named constants
- [ ] Dense shift/mask/ternary expressions split into named intermediates or helpers
- [ ] NULL checks use `!ptr` style
- [ ] Naming follows conventions (`module_action_noun()`, `snake_case` vars)

## Error Handling Review

Per `docs/ERROR_CODES_CATALOG.md`:

- [ ] Returns `ZR_OK` (0) for success
- [ ] Returns negative `ZR_ERR_*` codes for failures
- [ ] No partial effects on error paths
- [ ] Resources cleaned up on failure

## Testing Requirements

- [ ] New functionality has unit tests
- [ ] Binary format changes have golden fixtures
- [ ] Tests are deterministic (no wall-clock, no randomness)
- [ ] Integration tests for platform-specific code

## Forbidden Patterns

Check for absence of:

```c
// WRONG: OS header in core
#include <unistd.h>

// WRONG: Platform ifdef in core
#ifdef _WIN32

// WRONG: Forbidden libc in core
printf("debug: %d\n", x);

// WRONG: Unchecked arithmetic
size_t new_size = size + extra;  // might overflow

// WRONG: Reading without bounds check
uint32_t val = *(uint32_t*)ptr;

// WRONG: Magic number
if (align > 4096u) return false;

// WRONG: Partial effect before validation
buffer->len = new_len;  // before checking if valid
if (!valid) return ZR_ERR_INVALID_ARGUMENT;
```

## Review Report Format

```markdown
## Code Review: [file/module]

### Summary
[3 sentences max]

### Automated Checks
- Build: [PASS/FAIL]
- Guardrails: [PASS/FAIL]
- Tests: [PASS/FAIL]

### Issues Found
1. `file:line` - [issue description]
   ```c
   // Fix:
   [copyable fix]
   ```

### Recommendations
1. [Brief recommendation]
```
