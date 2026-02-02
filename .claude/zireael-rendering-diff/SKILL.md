---
name: zireael-rendering-diff
description: Implement framebuffer and diff renderer with grapheme safety and minimal terminal output.
metadata:
  short-description: Framebuffer + diff renderer
---

## When to use

Use this skill when working on:

- framebuffer cell model or double-buffering
- drawlist execution semantics
- diff renderer and terminal output emission
- cursor/style state tracking

## Source of truth

- `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md` — drawlist execution
- `docs/GOLDEN_FIXTURE_FORMAT.md` — golden test format
- `docs/VERSION_PINS.md` — pinned policies
- `src/core/zr_fb.h` — framebuffer interface
- `src/core/zr_diff.h` — diff renderer interface

## Architecture

- Offscreen framebuffer, double-buffered (prev/next)
- Cell model: Unicode grapheme-safe, colors, attrs
- Clipping stack (push/pop) respected by all draw ops
- Diff renderer:
    - computes minimal changes (prev → next)
    - tracks cursor/style to minimize escape sequences
    - dirty-line detection + span emission
    - builds output into one buffer, single flush per frame

## Grapheme safety invariants

- Never split a grapheme cluster across cells
- Never emit only half of a wide glyph
- Continuation cells represented explicitly (width=0)
- Diff expands spans safely around wide glyphs

## Diff renderer design

Pure callable for testing:

```c
zr_diff_render(prev_fb, next_fb, caps, initial_state) → emitted_bytes
```

Outputs:

- VT/ANSI escape sequences
- Cursor positioning (CUP)
- Style changes (SGR)
- Raw text bytes

## Golden testing

Fixtures must pin:

- capabilities (truecolor vs 256 vs 16)
- width policy mode
- initial cursor/style

Output compared byte-for-byte.
