# Quickstart

This page walks through the smallest “render + poll events” loop using the C engine.

## Build (POSIX: Linux/macOS)

```bash
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
```

## Run an example

```bash
./out/build/posix-clang-debug/zr_example_minimal_render_loop
```

What to expect:

- Clears the screen each frame.
- Renders a short status line.
- Exits when you press **Esc**.

## Wrapper loop (shape)

Zireael is designed around a wrapper-controlled frame loop:

```c
/* 1) poll events into caller buffer */
int n = engine_poll_events(e, timeout_ms, event_buf, (int)sizeof(event_buf));

/* 2) submit drawlist bytes */
engine_submit_drawlist(e, drawlist_bytes, drawlist_len);

/* 3) present (diff + single flush) */
engine_present(e);
```

## Next steps

- [Install & Build](install-build.md)
- [ABI Policy](../abi/abi-policy.md)
- [Examples → Input Echo](../examples/input-echo.md)

