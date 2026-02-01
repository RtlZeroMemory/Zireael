---
name: zireael-error-contracts
description: Keep ZR_ERR_* codes and "no partial effects" contracts consistent across the engine.
metadata:
  short-description: Error codes + state effects
---

## When to use

Use this skill when:

- adding/modifying public `engine_*` APIs
- writing parsers/validators (drawlist, input, UTF-8)
- changing cap enforcement (`zr_limits_t`)
- changing "no partial effects" behavior

## Source of truth

- `docs/ERROR_CODES_CATALOG.md` — error semantics and state effects
- `docs/SAFETY_RULESET.md` — safety and cleanup rules
- `src/util/zr_result.h` — actual error codes

## Core rules (must follow)

- `ZR_OK == 0`
- Failures are negative (`ZR_ERR_*`)
- Default: **no partial effects** on failure
- Only permitted partial output: event batch truncation (success with `TRUNCATED` flag)

## Error codes

| Code                      | Meaning                           |
|---------------------------|-----------------------------------|
| `ZR_ERR_INVALID_ARGUMENT` | NULL or invalid parameter         |
| `ZR_ERR_OOM`              | Allocation/arena growth failed    |
| `ZR_ERR_LIMIT`            | Caps exceeded or buffer too small |
| `ZR_ERR_FORMAT`           | Malformed input bytes             |
| `ZR_ERR_UNSUPPORTED`      | Unknown version/opcode/feature    |
| `ZR_ERR_PLATFORM`         | Backend/OS failure                |

## Implementation checklist

1. Identify failure classes your code can hit
2. Decide "state mutated?" explicitly
3. Validators run fully before any mutation
4. Writers never emit partial records

## Test guidance

- Invalid args → `ZR_ERR_INVALID_ARGUMENT`, no mutation
- Caps exceeded → `ZR_ERR_LIMIT`, no mutation
- Bad format → `ZR_ERR_FORMAT`/`ZR_ERR_UNSUPPORTED`, no partial effects
- Event truncation → success with `TRUNCATED` flag, only complete records
