# Minimal example

Zireael is designed for wrappers, but you can call the engine directly from C.

See `examples/example.c` for a complete program that:

- creates an engine with default config
- submits a drawlist
- presents output
- polls events

## Common integration pattern

- Pre-allocate an event output buffer (fixed size).
- Build drawlist bytes in your wrapper.
- Call:
  - `engine_submit_drawlist(e, bytes, len)`
  - `engine_present(e)`
  - `engine_poll_events(e, timeout_ms, out_buf, out_cap)`
