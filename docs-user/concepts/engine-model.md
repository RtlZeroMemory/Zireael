# Engine Model

Zireael's engine has two independent data paths: output (rendering) and input (events).

## Architecture

```
┌──────────────────┐         ┌─────────────────────────────────────────────┐
│ Wrapper          │         │                  Zireael                    │
│ (any language)   │         │─────────────────────────────────────────────│
│                  │ drawlist│  drawlist → framebuffer → diff → terminal   │
│ submit bytes ────┼────────▶│                                             │
│                  │         │  UTF-8 / graphemes / width calculation      │
│ poll events ◀────┼─────────│                                             │
│                  │  events │  POSIX / Win32 platform backends            │
└──────────────────┘         └─────────────────────────────────────────────┘
```

## Output Path

1. Wrapper builds [drawlist bytes](../abi/drawlist-v1.md)
2. Engine validates and executes into "next" framebuffer
3. Engine diffs "prev" vs "next", produces terminal escape sequences
4. Single flush to terminal
5. Swap: prev ← next

```
drawlist bytes
    │
    ▼
validate (bounds, caps, version)
    │
    ▼
execute → next framebuffer
    │
    ▼
diff(prev, next) → escape sequences → write (single flush)
    │
    ▼
swap(prev, next)
```

## Input Path

1. Platform reads terminal input bytes
2. Engine parses into normalized events (keys, mouse, resize)
3. Engine packs into [event batch](../abi/event-batch-v1.md)
4. Caller receives packed binary in provided buffer

```
terminal input → parse → normalize → queue
                                      │
                                      ▼
                           pack to event batch
                                      │
                                      ▼
                           caller buffer
```

## Key Invariants

- **Single flush**: One `write()` call per `engine_present()`
- **No partial effects**: Validation failure = no commands execute
- **Deterministic**: Same inputs + config = same outputs
