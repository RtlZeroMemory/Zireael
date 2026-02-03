# Module — Unicode Text

Unicode support is deterministic and does not depend on libc locale facilities.

## Version pin

Unicode 15.1.0 (`ZR_UNICODE_VERSION_MAJOR=15`, `MINOR=1`, `PATCH=0`).

This pin ensures identical grapheme segmentation and width calculation across all platforms and toolchains.

## Components

### UTF-8 decoding (`src/unicode/zr_utf8.*`)

- RFC-conformant UTF-8 decoder
- Pinned invalid-sequence policy: malformed bytes produce `U+FFFD`
- No locale dependencies

### Grapheme iteration (`src/unicode/zr_grapheme.*`)

- UAX #29 grapheme cluster boundaries (minimal subset)
- Iterator interface for walking grapheme boundaries in UTF-8 text
- Deterministic: same input bytes produce same grapheme breaks

### Width policy (`src/unicode/zr_width.*`)

- Terminal column width for codepoints and graphemes
- Output is always 0, 1, or 2 columns
- Emoji width is policy-dependent:
  - `ZR_WIDTH_EMOJI_NARROW` — emoji renders as 1 column
  - `ZR_WIDTH_EMOJI_WIDE` — emoji renders as 2 columns (default)

### Wrapping (`src/unicode/zr_wrap.*`)

- Line wrapping at grapheme boundaries
- Width-aware: respects wide characters

## Determinism guarantees

| Property | Guarantee |
|----------|-----------|
| Grapheme breaks | Identical for same input bytes |
| Width calculation | Identical for same codepoint + policy |
| Invalid sequences | Always produce `U+FFFD` |
| No locale | No `setlocale()`, no `LC_*` environment |

## Usage pattern

```c
// Iterate graphemes in UTF-8 text
zr_grapheme_iter_t iter;
zr_grapheme_iter_init(&iter, utf8_bytes, utf8_len);

zr_grapheme_t g;
while (zr_grapheme_next(&iter, &g)) {
    // g.offset, g.size identify grapheme bytes within the buffer
    const uint8_t* ptr = iter.bytes + g.offset;
    uint8_t w = zr_width_grapheme_utf8(ptr, g.size, policy);
    // w is 0, 1, or 2
}
```

See:

- `src/unicode/zr_utf8.h`
- `src/unicode/zr_grapheme.h`
- `src/unicode/zr_width.h`
- `src/unicode/zr_wrap.h`
