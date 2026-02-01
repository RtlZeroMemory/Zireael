---
name: zireael-unicode-text
description: Implement deterministic UTF-8, grapheme segmentation, width policy, and wrapping primitives.
metadata:
  short-description: Unicode correctness + policies
---

## When to use

Use this skill when implementing or modifying:

- UTF-8 decoder behavior
- grapheme segmentation rules
- width measurement policy (CJK/emoji/VS/ZWJ)
- wrapping and tab expansion

## Source of truth

- `docs/modules/UNICODE_TEXT.md` — module specification
- `docs/VERSION_PINS.md` — Unicode 15.1.0, emoji-wide policy, invalid UTF-8 policy

## Pinned policies (locked)

- **Unicode version**: 15.1.0
- **Emoji width**: wide (2 columns)
- **Invalid UTF-8**: emit U+FFFD, consume 1 byte, continue

## Implementation requirements

Must provide deterministic primitives:

- UTF-8 decode (invalid sequences handled deterministically)
- grapheme-safe ops (never split a cluster)
- column width measurement + wrapping

Keep Unicode code OS-header-free (`src/unicode/` + `src/util/` only).

## Invalid UTF-8 handling

- Never read past buffer bounds
- Make progress deterministically (consume at least 1 byte)
- Return replacement U+FFFD for invalid sequences
- Mark result as invalid for caller inspection

## Testing

Unit tests:

- UTF-8 valid/invalid sequences
- width edge cases (emoji, ZWJ, VS16)
- grapheme boundaries
- wrapping positions
- tab expansion

Fuzz tests:

- UTF-8 decoder: no crash/hang on arbitrary bytes
- Grapheme iterator: always makes progress
