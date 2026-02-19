# Module — Terminal Detection and Profiling

This module defines startup terminal probing used by `engine_create()` to build
a stable `zr_terminal_profile_t`.

## Scope and goals

- Identify terminal emulator family/version when possible.
- Probe render/input capabilities with bounded startup latency.
- Keep behavior deterministic and safe on malformed/truncated responses.
- Preserve baseline backend behavior when probing is unavailable or silent.

## Query batch (startup)

The engine sends one startup batch in this order:

1. `CSI > 0 q` (XTVERSION)
2. `CSI c` (DA1)
3. `CSI > c` (DA2)
4. `CSI ? 2026 $ p` (DECRQM sync update)
5. `CSI ? 2027 $ p` (DECRQM grapheme clusters)
6. `CSI ? 1016 $ p` (DECRQM pixel mouse)
7. `CSI ? 2004 $ p` (DECRQM bracketed paste)
8. `CSI 16 t` (cell pixel size)
9. `CSI 14 t` (text-area pixel size)

## Timeout and bounded behavior

- Per-read probe timeout: **100 ms max**
- Total probe budget per engine create: **500 ms max**
- If no response arrives, probing exits silently and falls back.

Backends expose timed reads through `plat_read_input_timed()`.

## Response parsing contract

Parser accepts responses in any order and ignores unknown bytes safely.

Handled response families:

- XTVERSION: `DCS > | ... ST`
- DA1: `CSI ? Ps ; ... c`
- DA2: `CSI > Pp ; Pv ; Pc c`
- DECRQM: `CSI ? mode ; value $ y`
- Metrics: `CSI 6 ; h ; w t` and `CSI 4 ; h ; w t`

Safety rules:

- Strict bounds checks on every read.
- Strict decimal parsing with overflow checks.
- Malformed fragments are ignored, never crash/hang.

## Identity and fallback

Primary identity source is XTVERSION payload parsing.

If XTVERSION is unavailable, backend hint fallback is used from environment
markers (for example Kitty/WezTerm/Ghostty/iTerm2/WT_SESSION/TERM_PROGRAM/TERM).

If probing is disabled (for example explicit non-terminal modes), identity
remains `ZR_TERM_UNKNOWN`.

## Known terminal defaults

A hardcoded conservative table maps known terminal IDs to non-queryable feature
defaults (for example kitty graphics, iTerm2 inline images, underline styles).

Table is intentionally conservative and can be expanded without changing parser
framing rules.

## Overrides

Create/runtime config exposes:

- `cap_force_flags`
- `cap_suppress_flags`

Precedence is fixed:

`effective = (detected | force) & ~suppress`

When the same bit appears in both masks, suppress wins.

## Backward compatibility

- `zr_terminal_profile_t` stores extended probe identity/capability details.
- Legacy `zr_terminal_caps_t` remains supported and is populated from effective
  runtime caps/profile state.

