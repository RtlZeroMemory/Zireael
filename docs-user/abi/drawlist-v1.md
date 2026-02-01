# Drawlist v1

The drawlist is a self-framed command stream consumed by the engine to update
an in-memory framebuffer.

## Source of truth

- ABI types: `include/zr/zr_drawlist.h`
- Spec: `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md`

## High-level structure

- `zr_dl_header_t` (fixed header; offsets and sizes)
- command stream: repeated `{zr_dl_cmd_header_t}{payload...}`
- string table: spans + concatenated UTF-8 bytes
- blob table: spans + concatenated bytes

## Unknown features policy (v1)

- Unknown opcodes are rejected with `ZR_ERR_UNSUPPORTED`.
