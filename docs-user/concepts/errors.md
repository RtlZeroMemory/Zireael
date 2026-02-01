# Error model

Zireael uses integer result codes:

- `0` means success (`ZR_OK`)
- negative values are failures (`ZR_ERR_*`)

Special case: `engine_poll_events()` uses an “int bytes written” convention:

- `> 0`: bytes written to `out_buf`
- `0`: no events before `timeout_ms`
- `< 0`: failure (negative `ZR_ERR_*`)

Error codes are defined in `docs/ERROR_CODES_CATALOG.md` and `src/util/zr_result.h`.
