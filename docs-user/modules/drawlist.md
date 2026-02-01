# Module: Drawlist (parse/validate/execute)

The drawlist is the wrapper → engine command stream that deterministically
updates the in-memory framebuffer.

## Source of truth

- ABI structs: `include/zr/zr_drawlist.h`
- Engine implementation: `src/core/zr_drawlist.c`, `src/core/zr_drawlist.h`
- Internal spec (normative): `docs/modules/DRAWLIST_FORMAT_AND_PARSER.md`

## Core rules

- The drawlist is a versioned, self-framed, little-endian byte stream.
- Offsets and sizes are validated before any pointer derivation.
- Commands are self-framed by `{opcode, flags, size}`.
- The engine borrows the drawlist bytes (no copying).

## Error behavior

Unknown opcodes in v1 are rejected with `ZR_ERR_UNSUPPORTED`.

Validation failures return a negative `ZR_ERR_*` and must not cause partial
effects (no framebuffer mutation on a rejected drawlist).
