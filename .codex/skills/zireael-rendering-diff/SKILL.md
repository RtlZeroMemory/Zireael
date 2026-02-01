---
name: zireael-rendering-diff
description: Implement framebuffer execution + diff renderer with grapheme safety, minimal terminal output, and golden-testable byte streams.
metadata:
  short-description: Framebuffer + diff renderer workflow
---

## When to use

Use this skill when working on:

- framebuffer cell model or double-buffering
- drawlist execution semantics (core rendering)
- diff renderer and terminal output emission
- scrolling acceleration (blit/terminal scroll)

## Requirements (locked docs)

Primary specs:

- `docs/modules/FRAMEBUFFER_MODEL_AND_OPS.md`
- `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md`
- `docs/modules/DIFF_RENDERER_AND_OUTPUT_EMITTER.md`
- `docs/GOLDEN_FIXTURE_FORMAT.md` (canonical golden storage/comparison)

- Offscreen framebuffer, double-buffered (prev/next).
- Cell model includes Unicode grapheme-safe representation, colors, and attrs.
- Clipping stack (push/pop) respected by all draw ops.
- Diff renderer:
  - computes minimal changes
  - tracks cursor/style state to minimize escape sequences
  - uses dirty-line detection and span emission
  - builds output into **one buffer** and flushes once per frame

## Grapheme safety invariants

- Never split a grapheme cluster across cells.
- Never emit only half of a wide glyph.
- Continuation cells must be represented explicitly (width=0) so diff can expand spans safely.

## Golden testing guidance

Design diff as a “pure” callable component for tests:

- `(prev_fb, next_fb, caps, initial_term_state) -> emitted_bytes`

Golden fixtures must pin:

- capabilities (truecolor vs 256 vs basic)
- width policy mode
- initial cursor/style assumptions
