# POSIX Backend

The POSIX backend (`src/platform/posix/`) supports Linux and macOS.

## Responsibilities

- **Terminal setup**: Raw mode via termios, disable echo and canonical mode
- **Input**: Read from stdin, handle SIGWINCH for resize
- **Output**: Write to stdout, single flush per `engine_present()`
- **Wake**: Self-pipe for cross-thread wakeup

## Capabilities Negotiated

| Capability | Detection |
|------------|-----------|
| Color mode | TERM/COLORTERM environment, terminfo |
| Mouse | Request SGR mouse mode |
| Bracketed paste | Request bracketed paste mode |
| Focus events | Request focus tracking |

## Limitations

- **Process-wide singleton**: Resize handler and wake pipe use global state
- **Signal handling**: SIGWINCH is caught; may conflict with application handlers

## Terminal Restoration

On `engine_destroy()`:

1. Restore original termios
2. Disable mouse mode
3. Show cursor
4. Exit alternate screen (if entered)
