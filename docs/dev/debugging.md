# Debugging

This guide focuses on practical triage for wrapper/runtime issues.

## Fast Triage Checklist

1. confirm runtime in a real tty/console session
2. verify requested versions and config are valid
3. inspect event batch bytes for framing errors
4. inspect drawlist validation errors before present
5. collect metrics and (optionally) debug trace snapshots

## Common Symptoms

### No output or garbled output

- verify terminal supports ANSI sequences
- confirm `engine_present()` return code is `ZR_OK`
- verify wrapper drawlist is valid and non-empty

### Input appears missing

- verify poll loop cadence (`engine_poll_events`) is running
- check event buffer capacity/truncation flag
- validate wrapper parser handles unknown event types by size skipping

### Resize not reflected

- ensure wrapper handles `ZR_EV_RESIZE`
- ensure redraw path rebuilds drawlist for new viewport

### Wrong key/mouse interpretation

- log raw event batch records (`type`, `size`, payload fields)
- compare with public event structs in `zr_event.h`

## Debug Trace API Workflow

1. enable trace (`engine_debug_enable`)
2. run scenario
3. query headers (`engine_debug_query`)
4. fetch payloads (`engine_debug_get_payload`)
5. inspect aggregate stats (`engine_debug_get_stats`)
6. optional export for offline analysis (`engine_debug_export`)

## Useful Runtime Signals

- metrics: bytes emitted, damage counts, event drop counts
- caps: negotiated terminal feature set
- event truncation flag: sustained pressure on wrapper event buffer

## Related Docs

- [Internal debug trace module](../modules/DEBUG_TRACE.md)
- [Testing](testing.md)
- [C ABI reference](../abi/c-abi-reference.md)
