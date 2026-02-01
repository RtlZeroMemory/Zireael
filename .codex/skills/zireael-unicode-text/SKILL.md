---
name: zireael-unicode-text
description: Implement deterministic UTF-8 decode, grapheme-safe segmentation, width policy, and wrapping primitives with strong tests.
metadata:
  short-description: Unicode correctness + policies
---

## When to use

Use this skill when implementing or modifying anything in `src/unicode/` or Unicode-dependent rendering logic:

- UTF-8 decoder behavior
- grapheme segmentation rules
- width measurement policy (CJK/emoji/VS/ZWJ)
- wrapping and tab expansion

## Requirements (from MASTERDOC.MD)

- Do not “DIY from vibes”: pick a Unicode helper approach early and document it.
- Define and enforce a width policy everywhere.
- Provide deterministic primitives:
  - UTF-8 decode (invalid sequences deterministic)
  - grapheme-safe ops (no half glyph)
  - column width measurement + wrapping

## Implementation guidance

- Keep Unicode code OS-header-free (`src/unicode` + `src/util` only).
- Prefer table-driven implementations with pinned Unicode version.
- For invalid UTF-8:
  - never read past buffer
  - make progress deterministically
  - return replacement `U+FFFD` by policy

## Testing guidance

Add/maintain tests that pin the chosen policy/version:

- unit: UTF-8 valid/invalid cases, width edge cases, grapheme boundaries, wrapping positions, tab expansion
- fuzz: UTF-8 decoder fuzz target (no crash/hang; deterministic behavior)

