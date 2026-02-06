# Module — Event System and Packed Event ABI

The engine outputs events as a packed, versioned, self-framed batch into a caller-provided buffer.

## Batch format (v1)

Defined by `src/core/zr_event.h`.

### Layout

```
┌─────────────────────────────┐
│ zr_evbatch_header_t  24B    │
├─────────────────────────────┤
│ Record 0 (header + payload) │
├─────────────────────────────┤
│ Record 1 ...                │
└─────────────────────────────┘
```

- Batch begins with `zr_evbatch_header_t` (magic, version, total_size, event_count, flags).
- Records are framed by `zr_ev_record_header_t.size` and 4-byte aligned.
- If the output buffer cannot fit the batch header, polling fails with `ZR_ERR_LIMIT`.
- If some records don't fit, the batch is truncated as a **success**: `ZR_EV_BATCH_TRUNCATED` flag is set, only complete records emitted.

## Event types

| Type | Value | Payload | Description |
|------|-------|---------|-------------|
| `ZR_EV_KEY` | 1 | 16B | Key press/release with key code, modifiers, action |
| `ZR_EV_TEXT` | 2 | 8B | Unicode scalar value input |
| `ZR_EV_PASTE` | 3 | 8B+ | Bracketed paste (UTF-8 bytes) |
| `ZR_EV_MOUSE` | 4 | 32B | Mouse move/drag/click/wheel with position and buttons |
| `ZR_EV_RESIZE` | 5 | 16B | Terminal resize (cols, rows) |
| `ZR_EV_TICK` | 6 | 16B | Frame tick with delta time |
| `ZR_EV_USER` | 7 | 16B+ | User-posted event via `engine_post_user_event()` |

`ZR_EV_TEXT` is emitted per decoded UTF-8 codepoint from platform input bytes.
Malformed UTF-8 is normalized to `U+FFFD` using the pinned invalid-sequence policy.

## Key event fields

- `key` — key code (escape, enter, arrows, F1-F12, etc.)
- `mods` — modifier mask (shift, ctrl, alt, meta)
- `action` — down, up, or repeat

## Text event fields

- `codepoint` — Unicode scalar value (U+0000..U+10FFFF, excluding surrogates)
- Invalid UTF-8 input is emitted as U+FFFD deterministically

## Mouse event fields

- `x`, `y` — cell position (signed)
- `kind` — move, drag, down, up, wheel
- `mods` — modifier mask
- `buttons` — button state bitmask
- `wheel_x`, `wheel_y` — scroll deltas (signed)

## Forward compatibility

- Unknown record types must be skippable by size.
- Record sizes must be validated against batch `total_size`.
- Wrappers should handle truncation gracefully and consider larger buffers if it occurs frequently.
