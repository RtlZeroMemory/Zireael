# Determinism

Zireael guarantees deterministic output across platforms and toolchains when:

- Inputs are identical (drawlists, input bytes)
- Configuration is identical (limits, policies)
- Version pins match

## Version Pins

| Component | Version | Purpose |
|-----------|---------|---------|
| Engine ABI | 1.0.0 | API stability |
| Drawlist format | v1 | Binary compatibility |
| Event batch format | v1 | Binary compatibility |
| Unicode data | 15.1.0 | Consistent text handling |

## What This Enables

- **Golden tests**: Compare byte-exact output across runs
- **Replay debugging**: Record inputs, reproduce issues
- **Cross-platform parity**: Same code, same behavior

## Caveats

Platform differences outside engine control:

- Terminal capabilities vary (color depth, mouse support)
- Timing depends on OS scheduler
- User events (`engine_post_user_event`) have caller-defined ordering
