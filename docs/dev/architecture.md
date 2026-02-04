# Architecture

Zireael is split into four layers:

- `src/util` — low-level containers, arenas, logging (OS-header-free)
- `src/unicode` — UTF-8 decode, graphemes, width, wrap (OS-header-free)
- `src/core` — engine loop, framebuffer, diff, drawlist, event pack/queue (OS-header-free)
- `src/platform/*` — OS interaction (termios/win32 console)

## Module map

For the authoritative module-level specs, see:

- [Internal Specs → Index](../00_INDEX.md)

## Next steps

- [Build System](build-system.md)
- [Testing](testing.md)

