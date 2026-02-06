# Quickstart

This page walks through the minimal wrapper-controlled loop:

1. poll events into a caller buffer
2. submit drawlist bytes
3. present once (diff + single flush)

## 1) Build

### POSIX (Linux/macOS)

```bash
cmake --preset posix-clang-debug
cmake --build --preset posix-clang-debug
```

### Windows (clang-cl)

```powershell
.\scripts\vsdev.ps1
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
```

## 2) Run The Minimal Example

### POSIX

```bash
./out/build/posix-clang-debug/zr_example_minimal_render_loop
```

### Windows

```powershell
.\out\build\windows-clangcl-debug\zr_example_minimal_render_loop.exe
```

What you should see:

- a basic status line rendered each frame
- responsive key input
- exit on `Esc`

## 3) Wrapper Loop Shape

```c
/* poll packed events into caller buffer */
int n = engine_poll_events(e, timeout_ms, event_buf, (int)sizeof(event_buf));
if (n < 0) {
  /* negative ZR_ERR_* */
}

/* submit drawlist bytes */
zr_result_t rc = engine_submit_drawlist(e, drawlist_bytes, drawlist_len);
if (rc != ZR_OK) {
  /* handle validation/limits/runtime failures */
}

/* emit diff output (single flush on success) */
rc = engine_present(e);
if (rc != ZR_OK) {
  /* handle platform/output failures */
}
```

## Recommended First Integration Checklist

- Use `zr_engine_config_default()` first, then override only required fields.
- Pin requested versions explicitly from `zr_version.h`.
- Keep event buffer at least 4 KiB to start; increase if truncation is frequent.
- Treat all incoming event bytes as untrusted and bounds-check before decoding.
- On any negative result code, avoid assuming partial output/state changes happened.

## Common Early Mistakes

- Assuming `engine_poll_events()` always blocks: it may return `0` on timeout.
- Assuming all unknown event types are errors: wrappers must skip unknown types by `size`.
- Writing drawlist payloads without zeroing reserved fields.

## Next Steps

- [Install & Build](install-build.md)
- [ABI Policy](../abi/abi-policy.md)
- [C ABI Reference](../abi/c-abi-reference.md)
- [Examples -> Input Echo](../examples/input-echo.md)
