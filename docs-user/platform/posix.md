# POSIX backend

The POSIX backend lives under `src/platform/posix/` and is responsible for:

- raw mode (termios)
- input reads and wakeups
- output writes (single flush per present)

Known limitation (current): the POSIX platform instance is process-wide
singleton because the resize handler and wake primitive use global state.

See `docs/modules/PLATFORM_INTERFACE.md`.
