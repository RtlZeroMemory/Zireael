# Module — Unicode Text

Unicode support is deterministic and does not depend on libc locale facilities.

## Components

- UTF-8 decoding (`src/unicode/zr_utf8.*`) with a pinned invalid-sequence policy.
- Grapheme iteration (`src/unicode/zr_grapheme.*`) implementing a minimal UAX #29 subset.
- Width policy (`src/unicode/zr_width.*`) with pinned emoji behavior.
- Wrapping (`src/unicode/zr_wrap.*`) operating at grapheme boundaries.

