# Header map

These headers are OS-header-free and safe to include from wrappers:

- `include/zr/zr_engine.h` — engine entrypoints
- `include/zr/zr_config.h` — config structs + validation
- `include/zr/zr_metrics.h` — metrics output struct
- `include/zr/zr_version.h` — pinned ABI/format versions
- `include/zr/zr_drawlist.h` — drawlist v1 ABI types
- `include/zr/zr_event.h` — event batch v1 ABI types

The platform boundary lives under `src/platform/` and is implemented by OS
backends.
