# Engine model

Zireael’s engine loop has two independent paths:

## Component map

```
+------------------+         +---------------------------------------------+
| Caller / Wrapper |         |                   Zireael                    |
|  (any language)  |         |---------------------------------------------|
|                  | drawlist|  core: drawlist → framebuffer → diff → emit  |
| submit bytes     |-------> |  unicode: UTF-8 / graphemes / width / wrap   |
|                  |         |  util: arenas / containers / checked math     |
| poll events      | <------ |  platform: POSIX / Win32 backend (raw I/O)    |
+------------------+         +---------------------------------------------+
```

## Output (rendering)

1. Wrapper builds drawlist bytes (v1 format).
2. Engine validates the drawlist and executes it into the “next” framebuffer.
3. Engine diffs “prev” vs “next” and produces terminal bytes.
4. Engine performs a **single flush** to the platform backend.
5. On success, engine swaps “prev ← next”.

Data flow:

```text
drawlist bytes
   │
   v
validate (bounds/caps/version)
   │
   v
execute → next framebuffer
   │
   v
diff(prev, next) → VT/ANSI byte stream → platform_write() (single flush)
   │
   v
swap(prev, next)
```

## Input (events)

1. Platform backend produces input bytes + resize/tick signals.
2. Engine parses and normalizes bytes to internal events.
3. Engine packs events into a caller-provided output buffer (event-batch v1).

Data flow:

```text
platform_read() → input bytes → parse/normalize → queue/coalesce
                                         │
                                         v
                               pack to event-batch ABI
                                         │
                                         v
                              caller-provided output buffer
```

See also:

- `docs/modules/DIFF_RENDERER_AND_OUTPUT_EMITTER.md`
- `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`
