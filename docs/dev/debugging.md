# Debugging

## Common issues

- **No output / garbled terminal:** ensure your terminal supports ANSI sequences and that you are running in a real TTY.
- **Missed resize:** ensure SIGWINCH is delivered (POSIX) and your wrapper polls events regularly.
- **Wrong key/mouse decoding:** capture raw input bytes and compare against the packed event batch.

## Next steps

- [Internal Specs → Debug Trace](../modules/DEBUG_TRACE.md)
- [Dev → Testing](testing.md)

