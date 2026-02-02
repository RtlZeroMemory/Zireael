# Drawlist v1

The drawlist is a binary command stream that drives rendering. Wrappers build drawlists and submit them via `engine_submit_drawlist()`.

## Layout

```
┌────────────────────────────────────────┐
│ Header (zr_dl_header_t)     64 bytes   │
├────────────────────────────────────────┤
│ Command stream                         │
│   cmd_header + payload                 │
│   cmd_header + payload                 │
│   ...                                  │
├────────────────────────────────────────┤
│ String spans (zr_dl_span_t[])          │
├────────────────────────────────────────┤
│ String bytes (UTF-8)                   │
├────────────────────────────────────────┤
│ Blob spans (zr_dl_span_t[])            │
├────────────────────────────────────────┤
│ Blob bytes                             │
└────────────────────────────────────────┘
```

## Header (64 bytes)

```
Offset  Size  Field                  Description
──────  ────  ─────                  ───────────
0x00    4     magic                  0x4C44525A ("ZRDL" little-endian)
0x04    4     version                1
0x08    4     header_size            64 (size of this header)
0x0C    4     total_size             Total drawlist size in bytes

0x10    4     cmd_offset             Byte offset to command stream
0x14    4     cmd_bytes              Command stream size in bytes
0x18    4     cmd_count              Number of commands

0x1C    4     strings_span_offset    Offset to string span array
0x20    4     strings_count          Number of string spans
0x24    4     strings_bytes_offset   Offset to string byte data
0x28    4     strings_bytes_len      String bytes total length

0x2C    4     blobs_span_offset      Offset to blob span array
0x30    4     blobs_count            Number of blob spans
0x34    4     blobs_bytes_offset     Offset to blob byte data
0x38    4     blobs_bytes_len        Blob bytes total length

0x3C    4     reserved0              Must be 0
```

## Command Header (8 bytes)

Each command starts with:

```
Offset  Size  Field    Description
──────  ────  ─────    ───────────
0x00    2     opcode   Command type (zr_dl_opcode_t)
0x02    2     flags    Reserved, must be 0
0x04    4     size     Total command size including header
```

## Opcodes

| Opcode | Value | Payload | Description |
|--------|-------|---------|-------------|
| `ZR_DL_OP_INVALID` | 0 | — | Invalid (rejected) |
| `ZR_DL_OP_CLEAR` | 1 | none | Clear framebuffer |
| `ZR_DL_OP_FILL_RECT` | 2 | 32 bytes | Fill rectangle with style |
| `ZR_DL_OP_DRAW_TEXT` | 3 | 40 bytes | Draw text from string table |
| `ZR_DL_OP_PUSH_CLIP` | 4 | 16 bytes | Push clipping rectangle |
| `ZR_DL_OP_POP_CLIP` | 5 | none | Pop clipping rectangle |
| `ZR_DL_OP_DRAW_TEXT_RUN` | 6 | 16 bytes | Draw pre-measured text run |

## Style (16 bytes)

Used by FILL_RECT and DRAW_TEXT:

```
Offset  Size  Field      Description
──────  ────  ─────      ───────────
0x00    4     fg         Foreground color (0x00RRGGBB)
0x04    4     bg         Background color
0x08    4     attrs      Attribute flags (bold, italic, etc.)
0x0C    4     reserved0  Must be 0
```

Attribute flags:

| Flag | Bit | Description |
|------|-----|-------------|
| Bold | 0 | Bold text |
| Italic | 1 | Italic text |
| Underline | 2 | Underlined text |
| Reverse | 3 | Swap fg/bg |
| Strikethrough | 4 | Strikethrough |

Color notes:

- Colors are always specified as RGB `0x00RRGGBB`.
- The engine will deterministically downgrade to 256-color or 16-color terminals as needed.

## Command Payloads

### FILL_RECT (32 bytes after header)

```
Offset  Size  Field   Description
──────  ────  ─────   ───────────
0x00    4     x       X position (signed)
0x04    4     y       Y position (signed)
0x08    4     w       Width
0x0C    4     h       Height
0x10    16    style   zr_dl_style_t
```

### DRAW_TEXT (40 bytes after header)

```
Offset  Size  Field         Description
──────  ────  ─────         ───────────
0x00    4     x             X position (signed)
0x04    4     y             Y position (signed)
0x08    4     string_index  Index into string span table
0x0C    4     byte_off      Byte offset within string
0x10    4     byte_len      Byte length to draw
0x14    16    style         zr_dl_style_t
0x24    4     reserved0     Must be 0
```

### PUSH_CLIP (16 bytes after header)

```
Offset  Size  Field  Description
──────  ────  ─────  ───────────
0x00    4     x      X position (signed)
0x04    4     y      Y position (signed)
0x08    4     w      Width
0x0C    4     h      Height
```

### DRAW_TEXT_RUN (16 bytes after header)

```
Offset  Size  Field       Description
──────  ────  ─────       ───────────
0x00    4     x           X position (signed)
0x04    4     y           Y position (signed)
0x08    4     blob_index  Index into blob span table
0x0C    4     reserved0   Must be 0
```

## Span Table Entry (8 bytes)

```
Offset  Size  Field  Description
──────  ────  ─────  ───────────
0x00    4     off    Byte offset into data section
0x04    4     len    Byte length
```

## Example: Minimal Drawlist

A drawlist that clears and draws "Hello":

```
Header (64 bytes):
  magic=0x4C44525A version=1 header_size=64 total_size=136
  cmd_offset=64 cmd_bytes=56 cmd_count=2
  strings_span_offset=120 strings_count=1
  strings_bytes_offset=128 strings_bytes_len=8
  blobs_span_offset=0 blobs_count=0
  blobs_bytes_offset=0 blobs_bytes_len=0
  reserved0=0

Command 0: CLEAR (8 bytes)
  opcode=1 flags=0 size=8

Command 1: DRAW_TEXT (48 bytes)
  opcode=3 flags=0 size=48
  x=0 y=0 string_index=0 byte_off=0 byte_len=5
  style: fg=0x00FFFFFF bg=0x00000000 attrs=0 reserved0=0
  reserved0=0

String spans (8 bytes):
  [0]: off=0 len=5

String bytes (8 bytes, padded to 4-byte alignment):
  "Hello\0\0\0"
```

Alignment rules enforced by validation:

- `total_size`, `cmd_bytes`, `strings_bytes_len`, `blobs_bytes_len` must be 4-byte aligned.
- All section offsets must be 4-byte aligned.
- Command sizes must be 4-byte aligned and match opcode expectations exactly.

## DRAW_TEXT_RUN blob format (v1)

`DRAW_TEXT_RUN` references a blob span. The blob payload is:

```
u32 seg_count
repeat seg_count times:
  zr_dl_style_t style   (16 bytes)
  u32 string_index
  u32 byte_off
  u32 byte_len
```

Total blob length must be exactly `4 + seg_count * 28`, and the blob span must be 4-byte aligned.

## Validation

The engine validates before execution:

- Magic and version match
- All offsets within `total_size`
- No overlapping sections
- Command sizes match opcode expectations
- String/blob indices within bounds
- Reserved fields are zero

On validation failure, `engine_submit_drawlist()` returns an error code and no commands execute.

## Unknown Opcodes

Unknown opcodes are rejected with `ZR_ERR_UNSUPPORTED`. This allows controlled feature additions in future versions.
