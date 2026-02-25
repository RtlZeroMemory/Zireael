# API Reference (Doxygen)

Public API headers are in `include/zr/`.

Published docs include a generated Doxygen section at:

- `.../api/` on the docs site

## Build Locally

```bash
bash scripts/docs.sh build
```

This performs:

- strict MkDocs build (`mkdocs build --strict`)
- Doxygen generation when `doxygen` is available
- copy of generated API HTML to `out/site/api/`

The script reuses `.venv-docs` and respects `$PYTHON`, so
`PYTHON=python3.11 bash scripts/docs.sh build` runs with a pinned interpreter
without rebuilding the virtualenv each time.

## Header Scope

Doxygen input is configured to include public headers under `include/zr/`.

Primary entrypoints:

- `zr_engine.h`
- `zr_config.h`
- `zr_result.h`
- `zr_drawlist.h`
- `zr_event.h`
- `zr_debug.h`

## Troubleshooting

- If API section is missing locally, verify `doxygen` is installed.
- If symbols are present but underspecified, improve comments in public headers.

## Next Steps

- [ABI -> C ABI Reference](abi/c-abi-reference.md)
- [Header layering spec](HEADER_LAYERING.md)
