# Terminal Correctness Audit (Zireael C Engine)

This report documents a terminal correctness audit focused on cases where Zireael’s internal framebuffer/cursor/style
model can diverge from real terminal behavior and cause visual artifacts, especially in strict terminals (Rio,
Alacritty).

Scope: engine core framebuffer + diff renderer output (no real terminal I/O, no timing dependencies).

## Pipeline Map (Prev/Next → Bytes)

1. Drawlist execution writes into the engine-owned `next` framebuffer (`zr_fb_t`).
2. The diff renderer computes `prev → next` changes and emits VT/ANSI bytes into a caller-provided output buffer:
   `src/core/zr_diff.c` (`zr_diff_render[_ex]`).
3. `engine_present()` performs a single platform flush on success, then swaps `prev ← next`.

Key internal contracts:

- Cells store one grapheme’s UTF-8 bytes + style + width (1, 2, or 0 for wide continuation).
- The diff renderer tracks a best-effort terminal state (`zr_term_state_t`) to minimize redundant CUP/SGR output.

## Findings Summary

| ID | Bucket | Symptom (Strict-Terminal Visible) | Root Cause | Status |
|---|---|---|---|---|
| C1 | C | Stale glyphs after resize when engine buffers are blank | No “screen contents valid” concept; sparse diffs can emit nothing | Fixed (new `SCREEN_VALID` + baseline clear) |
| D1 | D | Output stream corruption (cursor/style drift) when glyph bytes contain invalid UTF-8 or control scalars | Framebuffer stored original bytes; diff emitted verbatim | Fixed (sanitize to U+FFFD in framebuffer) |

Additional classes were audited and pinned with deterministic tests (no new code changes required in this PR): EOL shrink
clears trailing cells, wide-glyph removal clears both cells, attribute clears force absolute SGR, scroll-region side
effects (DECSTBM homes cursor), and cursor drift guard behavior for non-ASCII/wide cells.

## A) Cursor Movement Correctness (State Drift)

### A1) Blank-Cell Cursor Drift (Style Without Paint)

Symptom:

- A cell’s style changes (e.g., background color), but no printable bytes are emitted for that cell.
- Strict terminals will not advance the cursor or repaint the cell, causing cursor-state drift and/or stale background.

Why strict terminals expose it:

- Terminals only update the display when they receive printable characters (or erase ops). Emitting only SGR changes
  affects subsequent prints/erases, but does not repaint existing cells.

Root cause (historical class; verified guarded in current code):

- Any path that updates `ts.cursor_x` without emitting bytes for a width>0 cell.
- The diff renderer explicitly defends against “non-continuation empty cell” output by emitting spaces:
  `src/core/zr_diff.c:1170` (space-fill for `glyph_len==0` with `w>0`).
- The framebuffer also canonicalizes empty graphemes (`len==0`) to an ASCII space:
  `src/core/zr_framebuffer.c:787`.

Minimal reproduction scenario:

- `prev`: 1x1 cell is blank, style = black.
- `next`: 1x1 cell is “blank”, style = blue background.
- If the renderer emits only SGR and advances its cursor model, the terminal screen stays unchanged.

Fix plan / rationale:

- Ensure every width>0 cell causes output that advances the terminal cursor (glyph or spaces), or do not advance the
  tracked cursor.
- Zireael’s approach is to normalize blanks to spaces and space-fill any unexpected empty glyph cells.

Tests:

- Covered indirectly by the VT model invariant checks (see “F” below) and by golden fixtures that include style changes
  and wide-glyph cases. A dedicated “blank style repaint” golden can be added if desired.

### A2) Width Mismatch Drift Guard (Non-ASCII / Wide Glyphs)

Symptom:

- Some terminals/font stacks treat certain graphemes at widths that differ from the engine’s pinned model (emoji width,
  ambiguous-width codepoints, font fallback).
- If the renderer relies on sequential cursor tracking, later cells can land at the wrong column in strict terminals.

Root cause:

- The engine cannot guarantee that terminal column width == pinned model for all graphemes.

Mitigation in current implementation (verified):

- After emitting any wide cell (`width!=1`) or any glyph containing non-ASCII bytes, the diff renderer invalidates cached
  cursor position so the next cell is re-anchored with CUP:
  `src/core/zr_diff.c:168` (`zr_cell_may_drift_cursor`), applied at `src/core/zr_diff.c:1190`.

Tests:

- Enforced by the VT model: after “may drift” prints, further printable bytes without CUP are treated as a test failure.
  `tests/unit/zr_vt_model.c:156` and `tests/unit/zr_vt_model.c:166`.

## B) Style (SGR) Correctness (Style Drift)

### B1) Attribute Clears Must Use Reset-Based Absolute SGR

Symptom:

- Clearing attributes (bold/underline/reverse/...) using off-codes is terminal-dependent; emitting only add-codes can
  leave attributes stuck “on”, especially across terminal capability differences.

Why strict terminals expose it:

- Some terminals differ in which SGR “off” codes they support or how they interact with prior state; a renderer that
  emits only partial deltas can accumulate stale attributes that become visible as incorrect styling.

Root cause:

- “Delta SGR” logic that assumes availability/behavior of attribute-off codes.

Fix / rationale (already implemented; pinned):

- Zireael’s policy: any attribute clear falls back to reset-based absolute SGR (`0;...m`) to establish an exact style.
  `src/core/zr_diff.c:628` (`zr_emit_sgr_delta`).

Tests:

- Golden: `diff_011_attr_clear_forces_absolute_sgr` (bytes pinned) + VT model state check.

### B2) Avoid Using EL/ED Under Non-Default Style (Erase Fills With Current Attributes)

Symptom:

- Many terminals fill erased cells with the *current* attributes. Using EL/ED while style is non-default can paint the
  wrong background, leaving artifacts after shrink/scroll.

Audit result:

- The diff renderer does not use EL at all and only uses ED2 in the explicit “blank baseline” establishment path.
  `src/core/zr_diff.c:1101`.
- Trailing clears are done by painting spaces with the per-cell style, not by erase operations.

## C) Clearing & Residual Cells (Partial Redraw Artifacts)

### C1) Resize Preserves Terminal Contents (Blank Buffers Can’t Be Trusted)

Symptom:

- After a terminal resize, many terminals preserve existing on-screen glyphs.
- Zireael reallocates its internal framebuffers and clears them to spaces, so `prev`/`next` can both appear “blank”.
- A sparse diff can therefore emit no bytes, leaving stale terminal glyphs visible (strict terminals expose this clearly).

Root cause:

- No explicit “screen contents valid” concept in `zr_term_state_t`; only cursor/style validity was tracked.
- On resize, the engine invalidated style/cursor assumptions but still implicitly assumed screen contents matched `prev`.

File/line references:

- New flag bit: `src/core/zr_diff.h:54` (`ZR_TERM_STATE_SCREEN_VALID`).
- Baseline clear logic: `src/core/zr_diff.c:1116` (`zr_diff_establish_blank_screen_baseline`), called at
  `src/core/zr_diff.c:1799` when the bit is not set.
- Resize wiring: `src/core/zr_engine.c:561` clears the bit on resize.
- Init wiring: `src/core/zr_engine.c:1101` sets the bit after platform enter/raw-mode sequences.

Minimal reproduction scenario:

1. Terminal shows non-blank content (previous program output).
2. Engine receives resize; it reallocates buffers and clears them to spaces.
3. Next present is “blank → blank”; diff emits nothing.
4. Strict terminal still shows the old glyphs.

Fix plan / rationale:

- Add `ZR_TERM_STATE_SCREEN_VALID` to represent whether the terminal’s screen contents are known to match `prev`.
- When not valid, the renderer establishes a known blank baseline before sparse diffs:
  - reset scroll region (`ESC[r`, homes cursor)
  - emit a deterministic baseline style (absolute SGR)
  - clear screen (`ESC[2J`)
- This trades a small one-time byte cost for correctness and determinism.

Tests:

- Unit: `tests/unit/test_diff_term_state_validity.c` verifies the bit is set after rendering with screen invalid.
- Golden: `diff_008_screen_invalid_blank_baseline` pins emitted bytes and validates final tracked terminal state + screen.

### C2) EOL Shrink Clears Trailing Cells (Pinned)

Symptom:

- When a line becomes shorter, any previously-painted trailing cells must be cleared deterministically.

Why strict terminals expose it:

- Strict terminals preserve previous cell contents until explicitly overwritten. If the renderer only redraws the new
  shorter prefix, old trailing glyphs remain visible.

Audit result:

- Trailing cells that change from non-blank → blank are detected as dirty (`src/core/zr_diff.c:717`) and are repainted
  as spaces (normal glyph path) in the dirty span render (`src/core/zr_diff.c:1145`).

Tests:

- Golden: `diff_009_eol_shrink_clears_trailing_cells`.

### C3) Wide Glyph Removal Clears Lead + Continuation (Pinned)

Symptom:

- Clearing a wide glyph must clear both the lead and the continuation cell; otherwise a “phantom half” can remain.

Why strict terminals expose it:

- Terminals track wide-glyph occupancy per cell. Leaving a stale continuation cell can cause later prints to render with
  unexpected spacing or leave residual pixels/glyph fragments depending on font fallback.

Audit result:

- The diff renderer forces inclusion of dirty continuation-adjacent cells:
  - dirty continuation forces inclusion via `src/core/zr_diff.c:726`
  - damage spans are expanded around wide cells via `src/core/zr_diff.c:1208`
- Framebuffer writers preserve wide invariants (lead+continuation) and repair edge cases on overwrite:
  `src/core/zr_framebuffer.c:493`.

Tests:

- Golden: `diff_010_wide_glyph_removal_clears_pair`.

## D) Grapheme/Width Model Mismatches

### D1) Invalid UTF-8 / Control Scalars Must Not Reach the VT Stream

Symptom:

- If the renderer emits invalid UTF-8 bytes or Unicode control scalars as “glyph bytes”, strict terminals can interpret
  them as control input (ESC/CSI), corrupting the terminal state (cursor movement, mode changes), causing severe drift
  and visual artifacts.

Root cause:

- The grapheme iterator/decoder can identify invalid sequences (scalar = U+FFFD) while still exposing the *original*
  bytes.
- `zr_fb_put_grapheme()` previously stored bytes verbatim, and the diff renderer emits stored bytes verbatim.

File/line references:

- Sanitization: `src/core/zr_framebuffer.c:33` (`zr_fb_utf8_grapheme_bytes_safe_for_terminal`), enforced at
  `src/core/zr_framebuffer.c:807`.

Minimal reproduction scenario:

- Draw text that includes:
  - invalid UTF-8 byte `0x80` (standalone continuation)
  - ASCII ESC `0x1B`
- Without sanitization, these bytes can reach output and desynchronize the terminal.

Fix plan / rationale:

- Validate grapheme bytes are valid UTF-8 and reject C0/DEL/C1 control scalars.
- Replace any unsafe grapheme with U+FFFD (`EF BF BD`), width 1.
- This keeps the renderer “VT-stream safe” without requiring full output escaping.

Tests:

- Unit: `tests/unit/test_framebuffer_text.c` covers invalid UTF-8 and ASCII control replacement.

### D2) Ambiguous Width Policy (Pinned by Drift Guard)

Audit result:

- Zireael pins Unicode width/segmentation behavior and conservatively invalidates cursor position after any potentially
  width-ambiguous output (non-ASCII or wide), forcing CUP anchors before further prints.

## E) Scroll-Region Optimization Correctness

Audit result:

- Scroll optimization emits DECSTBM + SU/SD + DECSTBM reset and redraws newly exposed lines at full width.
- Cursor homing side effect of DECSTBM is modeled explicitly:
  `src/core/zr_diff.c:1054` and `src/core/zr_diff.c:1088`.
- Newly exposed lines are repainted full-width to avoid relying on terminal “blank insert” attributes:
  `src/core/zr_diff.c:1733`.

Tests:

- Golden: `diff_004_scroll_region_scroll_up_fullscreen` (bytes pinned).
- VT model applies the output and verifies resulting screen and term state match the renderer’s tracked final state.

## F) Output Correctness Invariants (Enforced in Tests)

Invariants made explicit and enforced deterministically:

1. Applying emitted bytes to a model terminal must produce a screen identical to `next`.
2. The renderer’s returned `final_state` must match the model’s inferred terminal state after applying bytes.
3. After any potentially width-drifting glyph, printing must be re-anchored with CUP before further printable output.

Implementation:

- Minimal VT model for tests: `tests/unit/zr_vt_model.c`, `tests/unit/zr_vt_model.h`.
- Golden harness now applies the VT model for each fixture:
  `tests/golden/golden_diff_renderer.c`.

## Performance Notes

- Baseline clear (`ESC[r` + baseline SGR + `ESC[2J`) is emitted only when `ZR_TERM_STATE_SCREEN_VALID` is not set.
  Normal steady-state frames are unchanged.
- UTF-8/control sanitization in `zr_fb_put_grapheme()` is bounded to `ZR_CELL_GLYPH_MAX` bytes per grapheme and avoids
  emitting terminal control bytes in the hot path.

## Open Risks / Not Covered

- This audit does not attempt full VT emulation in tests (tabs, insert mode, origin mode, DECAWM wrap-pending semantics).
  The renderer primarily uses CUP anchoring, which avoids most drift classes from these modes.
- Terminal/font-dependent glyph width differences remain possible; the renderer’s conservative CUP re-anchoring policy is
  the mitigation.
