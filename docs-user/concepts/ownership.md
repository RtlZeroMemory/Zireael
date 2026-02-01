# Ownership Model

Zireael uses a strict ownership model designed for FFI safety.

## Rules

1. **Engine owns its allocations** — Callers never free engine memory
2. **Callers own their buffers** — Engine borrows but does not retain
3. **No retained pointers** — Engine does not keep references past function return

## Buffer Ownership

| Function | Buffer | Owner |
|----------|--------|-------|
| `engine_submit_drawlist()` | drawlist bytes | Caller (must remain valid until `engine_present()`) |
| `engine_poll_events()` | event output buffer | Caller |
| `engine_post_user_event()` | payload bytes | Caller (copied during call) |

## Why This Matters for FFI

This model ensures:

- No double-free bugs across language boundaries
- No dangling pointers from GC-managed languages
- Predictable lifetime semantics
- Safe to call from any language with C FFI support
