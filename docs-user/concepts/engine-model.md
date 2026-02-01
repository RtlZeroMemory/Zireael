# Engine model

Zireael’s engine loop has two independent paths:

## Output (rendering)

1. Wrapper builds drawlist bytes (v1 format).
2. Engine validates the drawlist and executes it into the “next” framebuffer.
3. Engine diffs “prev” vs “next” and produces terminal bytes.
4. Engine performs a **single flush** to the platform backend.
5. On success, engine swaps “prev ← next”.

## Input (events)

1. Platform backend produces input bytes + resize/tick signals.
2. Engine parses and normalizes bytes to internal events.
3. Engine packs events into a caller-provided output buffer (event-batch v1).

See also:

- `docs/modules/DIFF_RENDERER_AND_OUTPUT_EMITTER.md`
- `docs/modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md`
