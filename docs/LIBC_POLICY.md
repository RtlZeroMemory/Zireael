# Zireael — libc Policy (Locked)

Zireael’s deterministic core avoids libc features that introduce locale, environment, time, randomness, or process variability.

## Enforced by CI

`scripts/guardrails.sh` enforces a forbidden call list under:

- `src/core/`
- `src/unicode/`
- `src/util/`

## Forbidden calls (core/unicode/util)

The following libc families are forbidden in the deterministic core:

- `printf` family: `printf`, `fprintf`, `sprintf`, `snprintf`, `vprintf`, `vfprintf`, `vsprintf`, `vsnprintf`, `dprintf`, `vdprintf`
- `puts`/`getchar` family: `puts`, `putchar`, `getchar`, `fputs`, `fputc`, `fgetc`
- `scanf` family: `scanf`, `fscanf`, `sscanf`, `vscanf`, `vfscanf`, `vsscanf`
- Locale / environment: `setlocale`, `getenv`, `putenv`
- Time / clock: `time`, `clock`, `difftime`, `mktime`, `localtime`, `gmtime`, `asctime`, `ctime`, `strftime`
- Randomness: `rand`, `srand`
- Process / shell: `system`, `popen`, `pclose`

## Allowed patterns

- `memcpy`/`memset`/`memcmp` and small, obvious libc primitives are allowed where they do not violate determinism.
- Platform backends (`src/platform/posix`, `src/platform/win32`) may use OS APIs and libc as needed, but must keep the core boundary clean.

