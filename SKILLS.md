# Zireael Skills Index

Skills are Codex-invocable prompts stored under `.codex/skills/`. Invoke with `$<skill-name>` or via `/skills`.

## Available Skills

| Skill                       | Use When                                                          |
|-----------------------------|-------------------------------------------------------------------|
| `zireael-spec-guardian`     | **Start here.** Any task affecting ABI, formats, or platform code |
| `zireael-code-style`        | Writing or reviewing C code for style, comments, constants        |
| `zireael-platform-boundary` | Working on platform interface or backends                         |
| `zireael-unicode-text`      | UTF-8, grapheme, width, or wrapping code                          |
| `zireael-abi-formats`       | Changing public API, drawlist, or event formats                   |
| `zireael-error-contracts`   | Adding error handling or "no partial effects" logic               |
| `zireael-rendering-diff`    | Framebuffer, diff renderer, or terminal output                    |
| `zireael-golden-fixtures`   | Adding or updating golden test fixtures                           |
| `zireael-header-layering`   | Refactoring includes or adding headers                            |
| `zireael-build-test-ci`     | CMake, testing, or CI configuration                               |

## Quick Start

For any non-trivial task, invoke the guardian first:

```
$zireael-spec-guardian
```

Then invoke domain-specific skills as needed.
