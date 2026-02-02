# Examples

This repo includes **working, up-to-date** examples in C and Go:

- `examples/example.c` — Minimal C embedding (engine create → submit drawlist → present).
- `poc/go-codex-tui/` — Stress-test TUI demo (scenarios + metrics + ABI exercise).

If you are implementing a wrapper, start from the ABI reference pages and then
use the examples as “known-good” integrations.

## C: Minimal Embedding

Build and run the C example:

- POSIX: `cmake --preset posix-clang-debug && cmake --build --preset posix-clang-debug && ./out/build/posix-clang-debug/zireael_example`
- Windows: `cmake --preset windows-clangcl-debug && cmake --build --preset windows-clangcl-debug && .\\out\\build\\windows-clangcl-debug\\zireael_example.exe`

The essential API flow:

```c
#include <stdint.h>
#include <stdio.h>

#include <zr/zr_config.h>
#include <zr/zr_engine.h>
#include <zr/zr_result.h>

int main(void) {
  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.requested_engine_abi_major = ZR_ENGINE_ABI_MAJOR;
  cfg.requested_engine_abi_minor = ZR_ENGINE_ABI_MINOR;
  cfg.requested_engine_abi_patch = ZR_ENGINE_ABI_PATCH;
  cfg.requested_drawlist_version = ZR_DRAWLIST_VERSION_V1;
  cfg.requested_event_batch_version = ZR_EVENT_BATCH_VERSION_V1;

  cfg.plat.requested_color_mode = PLAT_COLOR_MODE_RGB;
  cfg.plat.enable_mouse = 1u;

  zr_engine_t* e = NULL;
  zr_result_t rc = engine_create(&e, &cfg);
  if (rc != ZR_OK) {
    fprintf(stderr, "engine_create failed: %d\n", (int)rc);
    return 1;
  }

  /* Build a drawlist byte stream (see: ABI Reference → Drawlist v1). */
  /* engine_submit_drawlist(e, bytes, bytes_len); */
  /* engine_present(e); */

  engine_destroy(e);
  return 0;
}
```

Notes:

- Zireael expects **little-endian, bounds-checked drawlist bytes**. Do not cast a raw byte buffer to C structs; use explicit little-endian reads/writes.
- On POSIX, the current backend uses a process-global SIGWINCH wake handler and only allows **one active engine** at a time (a second `engine_create()` fails with `ZR_ERR_PLATFORM`).

## Go: Stress-test TUI Demo

The Go demo is a wrapper-style integration that intentionally pushes:

- drawlist throughput (many commands/strings per frame)
- diff output pressure (dirty line/col behavior)
- event polling
- metrics (`engine_get_metrics`)

Run it:

- POSIX: `bash scripts/poc-go-codex-tui.sh`
- Windows (native PowerShell): `powershell -ExecutionPolicy Bypass -File scripts\\poc-go-codex-tui.ps1`

Implementation references:

- POSIX cgo bridge: `poc/go-codex-tui/zr_cgo_unix.go`
- Windows DLL bridge: `poc/go-codex-tui/zr_windows.go`

For parsing packed events in a wrapper, use the safe reader approach in **ABI Reference → Event Batch v1**.
