# Glossary

- **Drawlist**: a versioned, little-endian byte stream of render commands submitted by the wrapper.
- **Event batch**: a versioned, little-endian byte stream of input events produced by the engine.
- **Framebuffer**: the engine’s logical grid of cells (text + style) used to compute diffs.
- **Diff renderer**: compares previous and next framebuffers and emits terminal escape sequences.
- **Caps / limits**: explicit bounds on bytes, counts, and per-frame output to ensure predictable work.

