# Drawlist Format (ZRDL v1)

ZRDL v1 is a 64-byte header plus a command stream. String/blob tables are no
longer carried per frame.

## Header

All header fields are little-endian `u32`.

- `magic`: `0x4C44525A` (`ZRDL`)
- `version`: `ZR_DRAWLIST_VERSION_V1` (`1`)
- `header_size`: `64`
- `total_size`: full buffer size
- `cmd_offset`: offset to command stream (normally `64`)
- `cmd_bytes`: command stream byte length
- `cmd_count`: command count
- `strings_span_offset`: `0`
- `strings_count`: `0`
- `strings_bytes_offset`: `0`
- `strings_bytes_len`: `0`
- `blobs_span_offset`: `0`
- `blobs_count`: `0`
- `blobs_bytes_offset`: `0`
- `blobs_bytes_len`: `0`
- `reserved0`: `0`

## Command Header

Every command starts with:

- `opcode` (`u16`)
- `flags` (`u16`, must be `0`)
- `size` (`u32`, includes header + payload)

Commands are 4-byte aligned.

## Opcodes

- `1` `ZR_DL_OP_CLEAR` (size `8`)
- `2` `ZR_DL_OP_FILL_RECT` (size `52`)
- `3` `ZR_DL_OP_DRAW_TEXT` (size `60`)
- `4` `ZR_DL_OP_PUSH_CLIP` (size `24`)
- `5` `ZR_DL_OP_POP_CLIP` (size `8`)
- `6` `ZR_DL_OP_DRAW_TEXT_RUN` (size `24`)
- `7` `ZR_DL_OP_SET_CURSOR` (size `20`)
- `8` `ZR_DL_OP_DRAW_CANVAS` (size `32`)
- `9` `ZR_DL_OP_DRAW_IMAGE` (size `40`)
- `10` `ZR_DL_OP_DEF_STRING` (base size `16` + aligned bytes)
- `11` `ZR_DL_OP_FREE_STRING` (size `12`)
- `12` `ZR_DL_OP_DEF_BLOB` (base size `16` + aligned bytes)
- `13` `ZR_DL_OP_FREE_BLOB` (size `12`)

## Persistent Resource Semantics

`DEF_*` commands define or overwrite engine-owned resources.

- `DEF_STRING`: `id`, `byte_len`, `bytes`, 0-padding to 4-byte alignment
- `FREE_STRING`: `id`
- `DEF_BLOB`: `id`, `byte_len`, `bytes`, 0-padding to 4-byte alignment
- `FREE_BLOB`: `id`

`DRAW_TEXT` references `string_id` + `byte_off` + `byte_len`.
`DRAW_TEXT_RUN`, `DRAW_CANVAS`, and `DRAW_IMAGE` reference blob IDs.

Unknown resource IDs at execute time are format errors.

## Validation Notes

Validator checks:

- magic/version/header size
- section offsets/sizes/alignment
- command framing (`cmd_bytes`, `cmd_count`, per-command `size`)
- zeroed reserved fields
- opcode-specific payload constraints

Executor is no-partial-effects at frame submit boundaries.
