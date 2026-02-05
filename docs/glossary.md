# Glossary

- **ABI (Application Binary Interface)**: stable binary contract between wrappers and engine C surface.
- **Drawlist**: versioned, little-endian command stream sent by wrapper to engine.
- **Event batch**: packed, little-endian event stream produced by engine for wrapper consumption.
- **Framebuffer**: logical cell grid representing rendered content before terminal diff emission.
- **Diff renderer**: compares previous and next framebuffer and emits minimal terminal output.
- **Damage rectangles**: tracked changed regions used to reduce work/output.
- **Caps / limits**: explicit deterministic bounds that constrain memory/work per frame.
- **Reserved field**: currently-unused ABI field that must be zero for forward compatibility.
- **No partial effects**: failure path guarantee that avoids committing half-applied state changes.
- **Single flush**: one backend write per successful `engine_present()` call.
