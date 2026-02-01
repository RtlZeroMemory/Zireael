# Win32 backend

The Win32 backend lives under `src/platform/win32/` and is responsible for:

- enabling VT processing
- reading input events
- output writes (single flush per present)

See `docs/modules/PLATFORM_INTERFACE.md`.
