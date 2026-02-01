# Zireael — Codex Skills (Repo-Scoped)

This repository includes **repo-scoped** Codex skills under `.codex/skills/`.

Per OpenAI’s agent skills format, each skill is a folder containing a required `SKILL.md` with YAML front matter (`name`, `description`) plus optional supporting files. Codex loads the skill name/description at startup and reads the full instructions only when invoked. citeturn0search0turn0search3

## How to use

- In Codex: type `$<skill-name>` (for example: `$zireael-spec-guardian`). citeturn0search2
- Or use `/skills` to select an available skill. citeturn0search0

## Skills in this repo

- `$zireael-spec-guardian` — MASTERDOC compliance + guardrails checklist for any change.
- `$zireael-platform-boundary` — platform layer boundary enforcement (no OS headers in core/unicode/util, `#ifdef` policy).
- `$zireael-abi-formats` — C ABI stability, version negotiation, packed event batches, binary drawlists (no TS code).
- `$zireael-unicode-text` — UTF-8 decoding, grapheme segmentation, width policy, wrapping primitives, Unicode version pinning.
- `$zireael-rendering-diff` — framebuffer model, drawlist execution semantics, diff renderer output minimization + golden hooks.
- `$zireael-build-test-ci` — CMake/toolchains, CI matrix, sanitizers, unit/golden/fuzz/integration testing strategy.

