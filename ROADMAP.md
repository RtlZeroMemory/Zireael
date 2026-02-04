# Roadmap

This roadmap is intentionally concrete: it lists tasks that can be reviewed against ABI/safety constraints.

## Near-term

- Docs: expand wrapper-facing ABI pages with more concrete binary examples (drawlist + event batch).
- CI: add version drift checks (header pins vs docs), and improve artifact capture on failures.
- Examples: keep minimal C examples in sync with public headers and supported versions.

## Medium-term

- Terminal compatibility: grow the integration test matrix (PTY/ConPTY) with known-terminal fixtures.
- Performance: additional diff renderer benchmarks and stress fixtures with bounded output.
- Tooling: better “golden fixture” authoring tools (still deterministic, still in-repo).

## Longer-term

- Drawlist format evolution for higher throughput while preserving strict validation.
- Wider terminal capability detection and graceful feature downgrades.
- Wrapper integration guides for common FFI hosts (Rust/Go/Python) without expanding the C API.

