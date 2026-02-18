# Drawlist Format

Wrappers submit rendering commands as a versioned, little-endian drawlist byte stream.

Authoritative implementation spec:

- [Internal Specs -> Drawlist Format and Parser](../modules/DRAWLIST_FORMAT_AND_PARSER.md)

## High-Level Layout

```text
+-------------------------------+
| zr_dl_header_t (64 bytes)     |
+-------------------------------+
| command stream                |
+-------------------------------+
| string spans (zr_dl_span_t[]) |
+-------------------------------+
| string bytes                  |
+-------------------------------+
| blob spans (zr_dl_span_t[])   |
+-------------------------------+
| blob bytes                    |
+-------------------------------+
```

All section offsets/sizes are validated before use.

## Header (`zr_dl_header_t`)

Required invariants:

- `magic == 0x4C44525A` (`'ZRDL'` little-endian)
- `header_size == 64`
- all section offsets/lengths are in-bounds within `total_size`
- section boundaries and command sizes are 4-byte aligned
- reserved fields are `0`

## Command Framing

Each command starts with:

- `u16 opcode`
- `u16 flags`
- `u32 size` (total bytes including header)

Unknown/unsupported opcodes are rejected with `ZR_ERR_UNSUPPORTED`.
Malformed size/layout is rejected with `ZR_ERR_FORMAT` or `ZR_ERR_LIMIT`.

## Opcodes and Sizes

| Opcode | Value | Version | Total size | Description |
|---|---:|---|---:|---|
| `ZR_DL_OP_CLEAR` | 1 | v1+ | 8 | clear framebuffer |
| `ZR_DL_OP_FILL_RECT` | 2 | v1+ | 40 | fill rect with style |
| `ZR_DL_OP_DRAW_TEXT` | 3 | v1+ | 48 | draw text slice from string table |
| `ZR_DL_OP_PUSH_CLIP` | 4 | v1+ | 24 | push clip rectangle |
| `ZR_DL_OP_POP_CLIP` | 5 | v1+ | 8 | pop clip rectangle |
| `ZR_DL_OP_DRAW_TEXT_RUN` | 6 | v1+ | 24 | draw segmented text run from blob |
| `ZR_DL_OP_SET_CURSOR` | 7 | v2+ | 20 | set desired cursor state |

## Style Encoding (`zr_dl_style_t`)

- `fg`, `bg`: `0x00RRGGBB`
- `attrs`: style bitmask (bold/italic/underline/reverse/dim/strike/overline/blink)
- `reserved0`: must be zero

Capability note:

- Runtime SGR capability mask from terminal caps may downgrade emitted attributes deterministically.

## String and Blob Tables

`zr_dl_span_t { off, len }` entries index into contiguous byte sections.

Wrappers should ensure:

- `string_index` / `blob_index` are in range
- `byte_off + byte_len` stays within referenced span
- UTF-8 payloads are valid when required by command semantics
- `DRAW_TEXT_RUN` blob validation is deterministic: span resolution, blob framing-size check, then per-segment slice checks

## Drawlist v2 Cursor Command

`ZR_DL_OP_SET_CURSOR` (`zr_dl_cmd_set_cursor_t`):

- `x`, `y`: 0-based cell coordinates, `-1` means "unchanged"
- `shape`: `0=block`, `1=underline`, `2=bar`
- `visible`: `0/1`
- `blink`: `0/1`
- `reserved0`: must be `0`

This command updates desired cursor state. It does not draw glyphs into the framebuffer.

## Validation Outcomes

Typical failure mapping:

- `ZR_ERR_FORMAT`: malformed structure, invalid command framing
- `ZR_ERR_LIMIT`: cap/bounds/alignment overflow violations
- `ZR_ERR_UNSUPPORTED`: unsupported version/opcode

On validation failure, command execution does not partially mutate presented frame state.

## Producer Checklist (Wrappers)

- Write all integers little-endian.
- Zero all reserved fields.
- Keep command/section offsets 4-byte aligned.
- Keep counts/lengths consistent with actual sections.
- Keep command sizes exact for each opcode.

## Next Steps

- [Event Batch Format](event-batch-format.md)
- [C ABI Reference](c-abi-reference.md)
- [Internal Drawlist Spec](../modules/DRAWLIST_FORMAT_AND_PARSER.md)
