# Zireael FFI Reference — TypeScript Engine Integration Guide

This document is the authoritative reference for building a TypeScript TUI engine on top of the Zireael C core via FFI (N-API/napi-rs).

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [ABI Versions and Pins](#abi-versions-and-pins)
4. [Error Codes](#error-codes)
5. [Engine API](#engine-api)
6. [Drawlist Format (v1)](#drawlist-format-v1)
7. [Event Batch Format (v1)](#event-batch-format-v1)
8. [Platform Capabilities](#platform-capabilities)
9. [Style and Colors](#style-and-colors)
10. [Unicode Handling](#unicode-handling)
11. [Memory Ownership](#memory-ownership)
12. [Threading Model](#threading-model)
13. [FFI Binding Patterns](#ffi-binding-patterns)
14. [TypeScript Integration Examples](#typescript-integration-examples)

---

## Overview

Zireael is a **deterministic terminal rendering engine** that:
- Accepts a **binary drawlist** describing what to render
- Returns a **packed event batch** with normalized input events
- Handles terminal I/O, raw mode, diff rendering, and Unicode internally

**Key properties:**
- All binary formats are **little-endian** and **4-byte aligned**
- **Caller-owned buffers** — no cross-FFI memory management
- **Integer return codes** — simple FFI marshaling
- **Deterministic** — same input + config = same output (pinned Unicode, no locale)

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     TypeScript TUI                          │
│  (Component tree, layout, reconciliation, event dispatch)   │
├─────────────────────────────────────────────────────────────┤
│                   DrawlistBuilder                           │
│  (Generates binary drawlist from virtual DOM)               │
├─────────────────────────────────────────────────────────────┤
│                  N-API / napi-rs Binding                    │
│  engine_create, engine_poll_events, engine_submit_drawlist  │
├─────────────────────────────────────────────────────────────┤
│                     Zireael C Core                          │
│  Drawlist → Framebuffer → Diff → VT/ANSI → Terminal         │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

**Rendering (TypeScript → Terminal):**
```
TypeScript VDOM
      ↓
DrawlistBuilder.build() → Uint8Array
      ↓
engine_submit_drawlist(bytes)
      ↓
[Zireael: validate → execute → framebuffer]
      ↓
engine_present()
      ↓
[Zireael: diff(prev, next) → VT bytes → terminal write]
```

**Events (Terminal → TypeScript):**
```
[Terminal input bytes]
      ↓
[Zireael: parse → normalize → queue → coalesce]
      ↓
engine_poll_events(timeout, buffer) → bytes_written
      ↓
parseEventBatch(buffer) → Event[]
      ↓
TypeScript event dispatch
```

---

## ABI Versions and Pins

```c
// src/core/zr_version.h
#define ZR_ENGINE_ABI_MAJOR       1
#define ZR_ENGINE_ABI_MINOR       0
#define ZR_ENGINE_ABI_PATCH       0

#define ZR_DRAWLIST_VERSION_V1    1
#define ZR_EVENT_BATCH_VERSION_V1 1
```

```c
// src/unicode/zr_unicode_pins.h
#define ZR_UNICODE_VERSION_MAJOR  15
#define ZR_UNICODE_VERSION_MINOR  1
#define ZR_UNICODE_VERSION_PATCH  0
```

**TypeScript constants:**
```typescript
const ZR_ENGINE_ABI_MAJOR = 1;
const ZR_DRAWLIST_VERSION_V1 = 1;
const ZR_EVENT_BATCH_VERSION_V1 = 1;
```

---

## Error Codes

All functions return `zr_result_t` (int32):

| Code | Name | Value | Meaning |
|------|------|-------|---------|
| `ZR_OK` | Success | `0` | Operation completed |
| `ZR_ERR_INVALID_ARGUMENT` | Invalid Argument | `-1` | NULL pointer, invalid enum, impossible value |
| `ZR_ERR_OOM` | Out of Memory | `-2` | Allocation failed |
| `ZR_ERR_LIMIT` | Limit Exceeded | `-3` | Buffer too small, cap exceeded |
| `ZR_ERR_UNSUPPORTED` | Unsupported | `-4` | Unknown version, opcode, feature |
| `ZR_ERR_FORMAT` | Format Error | `-5` | Malformed binary data |
| `ZR_ERR_PLATFORM` | Platform Error | `-6` | OS/terminal call failed |

**TypeScript enum:**
```typescript
enum ZrResult {
  OK = 0,
  ERR_INVALID_ARGUMENT = -1,
  ERR_OOM = -2,
  ERR_LIMIT = -3,
  ERR_UNSUPPORTED = -4,
  ERR_FORMAT = -5,
  ERR_PLATFORM = -6,
}
```

---

## Engine API

### Lifecycle

```c
// Create engine instance
zr_result_t engine_create(zr_engine_t** out_engine, const zr_engine_config_t* cfg);

// Destroy engine (idempotent)
void engine_destroy(zr_engine_t* e);
```

### Event Loop

```c
// Poll for events (blocking up to timeout_ms)
// Returns: >0 = bytes written, 0 = timeout, <0 = error
int engine_poll_events(zr_engine_t* e, int timeout_ms, uint8_t* out_buf, int out_cap);

// Post user event from any thread (wakes blocking poll)
zr_result_t engine_post_user_event(zr_engine_t* e, uint32_t tag,
                                   const uint8_t* payload, int payload_len);
```

### Rendering

```c
// Submit drawlist for next frame (validates, executes into framebuffer)
zr_result_t engine_submit_drawlist(zr_engine_t* e, const uint8_t* bytes, int bytes_len);

// Present: diff prev/next framebuffer, emit VT bytes, swap buffers
zr_result_t engine_present(zr_engine_t* e);
```

### Metrics/Config

```c
zr_result_t engine_get_metrics(zr_engine_t* e, zr_metrics_t* out_metrics);
zr_result_t engine_set_config(zr_engine_t* e, const zr_engine_runtime_config_t* cfg);
```

---

## Drawlist Format (v1)

The drawlist is a binary command buffer sent from TypeScript to the engine.

### Magic and Version

```
Magic: 0x5A52444C ('ZRDL' as little-endian u32)
Version: 1
```

### Header Layout (64 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `magic` | `0x5A52444C` |
| 4 | 4 | `version` | `1` |
| 8 | 4 | `header_size` | `64` |
| 12 | 4 | `total_size` | Total bytes including header |
| 16 | 4 | `cmd_offset` | Offset to command stream |
| 20 | 4 | `cmd_bytes` | Size of command stream |
| 24 | 4 | `cmd_count` | Number of commands |
| 28 | 4 | `strings_span_offset` | Offset to string span table |
| 32 | 4 | `strings_count` | Number of strings |
| 36 | 4 | `strings_bytes_offset` | Offset to string data |
| 40 | 4 | `strings_bytes_len` | Size of string data |
| 44 | 4 | `blobs_span_offset` | Offset to blob span table |
| 48 | 4 | `blobs_count` | Number of blobs |
| 52 | 4 | `blobs_bytes_offset` | Offset to blob data |
| 56 | 4 | `blobs_bytes_len` | Size of blob data |
| 60 | 4 | `reserved0` | Must be `0` |

### Overall Structure

```
┌────────────────────────────────────────┐
│ Header (64 bytes)                      │
├────────────────────────────────────────┤
│ Command Stream                         │
│   [{opcode, flags, size} payload]...   │
├────────────────────────────────────────┤
│ String Span Table                      │
│   [{off, len}, {off, len}, ...]        │
├────────────────────────────────────────┤
│ String Bytes (UTF-8)                   │
├────────────────────────────────────────┤
│ Blob Span Table                        │
│   [{off, len}, {off, len}, ...]        │
├────────────────────────────────────────┤
│ Blob Bytes                             │
└────────────────────────────────────────┘
```

### Command Header (8 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | `opcode` | Command type |
| 2 | 2 | `flags` | Must be `0` in v1 |
| 4 | 4 | `size` | Total command size (header + payload) |

### Opcodes

| Opcode | Name | Payload Size | Description |
|--------|------|--------------|-------------|
| 0 | `INVALID` | - | Invalid (rejected) |
| 1 | `CLEAR` | 0 | Clear framebuffer |
| 2 | `FILL_RECT` | 32 | Fill rectangle with style |
| 3 | `DRAW_TEXT` | 40 | Draw text from string table |
| 4 | `PUSH_CLIP` | 16 | Push clip rectangle |
| 5 | `POP_CLIP` | 0 | Pop clip rectangle |
| 6 | `DRAW_TEXT_RUN` | 16 | Draw styled text run from blob |

### Style (16 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `fg` | Foreground color `0x00RRGGBB` |
| 4 | 4 | `bg` | Background color `0x00RRGGBB` |
| 8 | 4 | `attrs` | Attribute bitmask |
| 12 | 4 | `reserved0` | Must be `0` |

### Span (8 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `off` | Offset into payload section |
| 4 | 4 | `len` | Length in bytes |

### Command Payloads

**CLEAR (opcode=1):** No payload

**FILL_RECT (opcode=2, 32 bytes):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `x` (int32) |
| 4 | 4 | `y` (int32) |
| 8 | 4 | `w` (int32) |
| 12 | 4 | `h` (int32) |
| 16 | 16 | `style` |

**DRAW_TEXT (opcode=3, 40 bytes):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `x` (int32) |
| 4 | 4 | `y` (int32) |
| 8 | 4 | `string_index` |
| 12 | 4 | `byte_off` |
| 16 | 4 | `byte_len` |
| 20 | 16 | `style` |
| 36 | 4 | `reserved0` (must be 0) |

**PUSH_CLIP (opcode=4, 16 bytes):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `x` (int32) |
| 4 | 4 | `y` (int32) |
| 8 | 4 | `w` (int32) |
| 12 | 4 | `h` (int32) |

**POP_CLIP (opcode=5):** No payload

**DRAW_TEXT_RUN (opcode=6, 16 bytes):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `x` (int32) |
| 4 | 4 | `y` (int32) |
| 8 | 4 | `blob_index` |
| 12 | 4 | `reserved0` (must be 0) |

---

## Event Batch Format (v1)

Events are returned from `engine_poll_events` as a packed binary batch.

### Magic and Version

```
Magic: 0x5A524556 ('ZREV' as little-endian u32)
Version: 1
```

### Batch Header (24 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `magic` | `0x5A524556` |
| 4 | 4 | `version` | `1` |
| 8 | 4 | `total_size` | Total bytes including header |
| 12 | 4 | `event_count` | Number of event records |
| 16 | 4 | `flags` | Batch flags |
| 20 | 4 | `reserved0` | Must be `0` |

**Flags:**
| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `TRUNCATED` | Some events were dropped (buffer too small) |

### Event Record Header (16 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `type` | Event type enum |
| 4 | 4 | `size` | Total record size (header + payload, 4-byte aligned) |
| 8 | 4 | `time_ms` | Monotonic timestamp |
| 12 | 4 | `flags` | Event-specific flags |

### Event Types

| Type | Name | Payload Size | Description |
|------|------|--------------|-------------|
| 0 | `INVALID` | - | Invalid |
| 1 | `KEY` | 16 | Key press/release |
| 2 | `TEXT` | 8 | Text input (codepoint) |
| 3 | `PASTE` | 8+ | Bracketed paste content |
| 4 | `MOUSE` | 32 | Mouse event |
| 5 | `RESIZE` | 16 | Terminal resize |
| 6 | `TICK` | 16 | Timer tick |
| 7 | `USER` | 16+ | User-posted event |

### Event Payloads

**KEY (type=1, 16 bytes):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `key` (zr_key_t enum) |
| 4 | 4 | `mods` (modifier bitmask) |
| 8 | 4 | `action` (1=down, 2=up, 3=repeat) |
| 12 | 4 | `reserved0` |

**TEXT (type=2, 8 bytes):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `codepoint` (Unicode scalar) |
| 4 | 4 | `reserved0` |

**PASTE (type=3, 8 bytes + data):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `byte_len` |
| 4 | 4 | `reserved0` |
| 8 | N | UTF-8 bytes (padded to 4-byte alignment) |

**MOUSE (type=4, 32 bytes):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `x` (int32, cell column) |
| 4 | 4 | `y` (int32, cell row) |
| 8 | 4 | `kind` (1=move, 2=drag, 3=down, 4=up, 5=wheel) |
| 12 | 4 | `mods` (modifier bitmask) |
| 16 | 4 | `buttons` (button bitmask) |
| 20 | 4 | `wheel_x` (int32) |
| 24 | 4 | `wheel_y` (int32) |
| 28 | 4 | `reserved0` |

**RESIZE (type=5, 16 bytes):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `cols` |
| 4 | 4 | `rows` |
| 8 | 4 | `reserved0` |
| 12 | 4 | `reserved1` |

**TICK (type=6, 16 bytes):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `dt_ms` (delta time since last tick) |
| 4 | 4 | `reserved0` |
| 8 | 4 | `reserved1` |
| 12 | 4 | `reserved2` |

**USER (type=7, 16 bytes + data):**
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `tag` (user-defined) |
| 4 | 4 | `byte_len` |
| 8 | 4 | `reserved0` |
| 12 | 4 | `reserved1` |
| 16 | N | payload bytes (padded to 4-byte alignment) |

### Key Codes

```typescript
enum ZrKey {
  UNKNOWN = 0,
  ESCAPE = 1,
  ENTER = 2,
  TAB = 3,
  BACKSPACE = 4,
  INSERT = 10,
  DELETE = 11,
  HOME = 12,
  END = 13,
  PAGE_UP = 14,
  PAGE_DOWN = 15,
  UP = 20,
  DOWN = 21,
  LEFT = 22,
  RIGHT = 23,
  F1 = 100, F2 = 101, F3 = 102, F4 = 103,
  F5 = 104, F6 = 105, F7 = 106, F8 = 107,
  F9 = 108, F10 = 109, F11 = 110, F12 = 111,
}
```

### Modifier Bitmask

```typescript
enum ZrMod {
  SHIFT = 1 << 0,  // 0x01
  CTRL  = 1 << 1,  // 0x02
  ALT   = 1 << 2,  // 0x04
  META  = 1 << 3,  // 0x08
}
```

---

## Platform Capabilities

The engine negotiates capabilities with the terminal at startup.

### Color Modes

| Value | Name | Description |
|-------|------|-------------|
| 0 | `UNKNOWN` | Unknown/unsupported |
| 1 | `16` | 16-color (ANSI) |
| 2 | `256` | 256-color (xterm) |
| 3 | `RGB` | 24-bit true color |

### Capability Flags

```typescript
interface PlatCaps {
  colorMode: 0 | 1 | 2 | 3;
  supportsMouse: boolean;
  supportsBracketedPaste: boolean;
  supportsFocusEvents: boolean;
  supportsOsc52: boolean;  // Clipboard
  sgrAttrsSupported: number;  // Bitmask of supported text attrs
}
```

---

## Style and Colors

### Color Format

Colors are 24-bit RGB packed as `0x00RRGGBB`:

```typescript
function rgb(r: number, g: number, b: number): number {
  return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

// Examples
const WHITE = 0x00FFFFFF;
const BLACK = 0x00000000;
const RED   = 0x00FF0000;
```

### Text Attributes

Bitmask flags for text styling:

| Bit | Value | Attribute |
|-----|-------|-----------|
| 0 | `0x01` | Bold |
| 1 | `0x02` | Italic |
| 2 | `0x04` | Underline |
| 3 | `0x08` | Reverse |
| 4 | `0x10` | Strikethrough |

```typescript
enum ZrAttr {
  BOLD       = 1 << 0,
  ITALIC     = 1 << 1,
  UNDERLINE  = 1 << 2,
  REVERSE    = 1 << 3,
  STRIKE     = 1 << 4,
}

// Combine attributes
const boldItalic = ZrAttr.BOLD | ZrAttr.ITALIC;
```

### Color Downgrade

The diff renderer automatically downgrades colors based on terminal capabilities:
- **RGB mode**: Colors used as-is
- **256 mode**: RGB → nearest xterm-256 color (cube + grayscale)
- **16 mode**: RGB → nearest ANSI 16 color

---

## Unicode Handling

### Pinned Behavior

- **Unicode version**: 15.1.0 (locked)
- **Emoji width policy**: Wide (2 cells) by default
- **Invalid UTF-8**: Replaced with U+FFFD, consume 1 byte

### Grapheme Clusters

The engine renders at **grapheme cluster** boundaries (UAX #29):
- Emoji with modifiers (👋🏽) → single cell
- Family emoji (👨‍👩‍👧‍👦) → single cell
- Flag emoji (🇺🇸) → single cell
- Combining marks → combined with base

### Cell Storage

Each framebuffer cell stores up to 32 bytes of UTF-8 (one grapheme cluster).
Graphemes exceeding 32 bytes are replaced with U+FFFD.

### Width Calculation

```
ASCII/Latin: 1 cell
CJK: 2 cells
Emoji: 2 cells (with wide policy)
Combining marks: 0 cells (combined with previous)
Control characters: 0 cells (skipped)
```

---

## Memory Ownership

### Caller-Owned Buffers

| Buffer | Owner | Lifetime |
|--------|-------|----------|
| Drawlist bytes | Caller | Until `engine_submit_drawlist` returns |
| Event output buffer | Caller | Caller manages |
| User event payload | Caller | Copied during `engine_post_user_event` |

### Engine-Owned State

| State | Owner | Notes |
|-------|-------|-------|
| Engine struct | Engine | Created by `engine_create`, freed by `engine_destroy` |
| Framebuffers (×2) | Engine | Allocated internally |
| Event queue | Engine | Bounded, internal |
| Platform handle | Engine | OS resources |

### No Cross-Boundary Allocation

The engine **never** returns pointers that require caller `free()`.
All data exchange is via caller-provided buffers with explicit sizes.

---

## Threading Model

### Single-Threaded Core

All `engine_*` functions except `engine_post_user_event` must be called from the **engine thread** (typically the main thread).

### Thread-Safe Wake

`engine_post_user_event` is **thread-safe** and will wake a blocking `engine_poll_events`:

```typescript
// Main thread
const bytes = engine.pollEvents(1000, buffer);  // Blocks

// Worker thread (or signal handler)
engine.postUserEvent(42, payload);  // Wakes the poll
```

### No Callbacks

The engine does not invoke user callbacks. All communication is via:
- Return values
- Caller-provided output buffers
- Caller polling for events

---

## FFI Binding Patterns

### N-API Binding Structure

```typescript
// Native module interface
interface ZiraelNative {
  engineCreate(config: Buffer): number;  // Returns handle or error
  engineDestroy(handle: number): void;
  enginePollEvents(handle: number, timeoutMs: number, buffer: Buffer): number;
  enginePostUserEvent(handle: number, tag: number, payload: Buffer): number;
  engineSubmitDrawlist(handle: number, bytes: Buffer): number;
  enginePresent(handle: number): number;
}
```

### Buffer Allocation

```typescript
// Pre-allocate buffers to avoid per-frame allocation
const eventBuffer = Buffer.alloc(64 * 1024);  // 64KB for events
const drawlistBuffer = Buffer.alloc(256 * 1024);  // 256KB for drawlist
```

### Error Handling

```typescript
function checkResult(result: number, operation: string): void {
  if (result < 0) {
    const errorNames: Record<number, string> = {
      [-1]: 'INVALID_ARGUMENT',
      [-2]: 'OOM',
      [-3]: 'LIMIT',
      [-4]: 'UNSUPPORTED',
      [-5]: 'FORMAT',
      [-6]: 'PLATFORM',
    };
    throw new Error(`${operation} failed: ${errorNames[result] ?? result}`);
  }
}
```

---

## TypeScript Integration Examples

### Engine Wrapper Class

```typescript
class ZiraelEngine {
  private handle: number;
  private eventBuffer: Buffer;

  constructor(config: EngineConfig) {
    this.eventBuffer = Buffer.alloc(64 * 1024);
    const configBuf = this.encodeConfig(config);
    this.handle = native.engineCreate(configBuf);
    checkResult(this.handle, 'engineCreate');
  }

  destroy(): void {
    native.engineDestroy(this.handle);
  }

  pollEvents(timeoutMs: number): Event[] {
    const bytesWritten = native.enginePollEvents(
      this.handle, timeoutMs, this.eventBuffer
    );
    if (bytesWritten < 0) {
      checkResult(bytesWritten, 'pollEvents');
    }
    if (bytesWritten === 0) {
      return [];  // Timeout
    }
    return this.parseEventBatch(this.eventBuffer.subarray(0, bytesWritten));
  }

  submitDrawlist(bytes: Uint8Array): void {
    const result = native.engineSubmitDrawlist(this.handle, Buffer.from(bytes));
    checkResult(result, 'submitDrawlist');
  }

  present(): void {
    const result = native.enginePresent(this.handle);
    checkResult(result, 'present');
  }

  postUserEvent(tag: number, payload: Uint8Array): void {
    const result = native.enginePostUserEvent(
      this.handle, tag, Buffer.from(payload)
    );
    checkResult(result, 'postUserEvent');
  }
}
```

### Drawlist Builder

```typescript
class DrawlistBuilder {
  private commands: Uint8Array[] = [];
  private strings: Map<string, number> = new Map();
  private stringData: Uint8Array[] = [];
  private stringSpans: Array<{off: number, len: number}> = [];
  private stringOffset = 0;

  clear(): this {
    this.addCommand(1, new Uint8Array(0));  // CLEAR
    return this;
  }

  fillRect(x: number, y: number, w: number, h: number, style: Style): this {
    const payload = new Uint8Array(32);
    const view = new DataView(payload.buffer);
    view.setInt32(0, x, true);
    view.setInt32(4, y, true);
    view.setInt32(8, w, true);
    view.setInt32(12, h, true);
    this.writeStyle(view, 16, style);
    this.addCommand(2, payload);  // FILL_RECT
    return this;
  }

  drawText(x: number, y: number, text: string, style: Style): this {
    const stringIndex = this.internString(text);
    const payload = new Uint8Array(40);
    const view = new DataView(payload.buffer);
    view.setInt32(0, x, true);
    view.setInt32(4, y, true);
    view.setUint32(8, stringIndex, true);
    view.setUint32(12, 0, true);  // byte_off
    view.setUint32(16, this.stringSpans[stringIndex].len, true);  // byte_len
    this.writeStyle(view, 20, style);
    view.setUint32(36, 0, true);  // reserved
    this.addCommand(3, payload);  // DRAW_TEXT
    return this;
  }

  pushClip(x: number, y: number, w: number, h: number): this {
    const payload = new Uint8Array(16);
    const view = new DataView(payload.buffer);
    view.setInt32(0, x, true);
    view.setInt32(4, y, true);
    view.setInt32(8, w, true);
    view.setInt32(12, h, true);
    this.addCommand(4, payload);  // PUSH_CLIP
    return this;
  }

  popClip(): this {
    this.addCommand(5, new Uint8Array(0));  // POP_CLIP
    return this;
  }

  build(): Uint8Array {
    // Calculate sizes and offsets
    const headerSize = 64;
    const cmdBytes = this.commands.reduce((sum, c) => sum + c.length, 0);
    const stringsSpanBytes = this.stringSpans.length * 8;
    const stringsBytesLen = align4(this.stringOffset);

    const cmdOffset = headerSize;
    const stringsSpanOffset = cmdOffset + align4(cmdBytes);
    const stringsBytesOffset = stringsSpanOffset + stringsSpanBytes;
    const totalSize = stringsBytesOffset + stringsBytesLen;

    // Build buffer
    const buffer = new Uint8Array(totalSize);
    const view = new DataView(buffer.buffer);

    // Header
    view.setUint32(0, 0x5A52444C, true);  // magic 'ZRDL'
    view.setUint32(4, 1, true);  // version
    view.setUint32(8, 64, true);  // header_size
    view.setUint32(12, totalSize, true);
    view.setUint32(16, cmdOffset, true);
    view.setUint32(20, cmdBytes, true);
    view.setUint32(24, this.commands.length, true);
    view.setUint32(28, stringsSpanOffset, true);
    view.setUint32(32, this.stringSpans.length, true);
    view.setUint32(36, stringsBytesOffset, true);
    view.setUint32(40, this.stringOffset, true);
    // blobs not used in this example
    view.setUint32(60, 0, true);  // reserved

    // Commands
    let offset = cmdOffset;
    for (const cmd of this.commands) {
      buffer.set(cmd, offset);
      offset += cmd.length;
    }

    // String spans
    offset = stringsSpanOffset;
    for (const span of this.stringSpans) {
      view.setUint32(offset, span.off, true);
      view.setUint32(offset + 4, span.len, true);
      offset += 8;
    }

    // String data
    offset = stringsBytesOffset;
    for (const data of this.stringData) {
      buffer.set(data, offset);
      offset += data.length;
    }

    return buffer;
  }

  private addCommand(opcode: number, payload: Uint8Array): void {
    const size = 8 + payload.length;  // header + payload
    const cmd = new Uint8Array(align4(size));
    const view = new DataView(cmd.buffer);
    view.setUint16(0, opcode, true);
    view.setUint16(2, 0, true);  // flags
    view.setUint32(4, size, true);
    cmd.set(payload, 8);
    this.commands.push(cmd);
  }

  private internString(text: string): number {
    const existing = this.strings.get(text);
    if (existing !== undefined) return existing;

    const bytes = new TextEncoder().encode(text);
    const index = this.stringSpans.length;
    this.strings.set(text, index);
    this.stringSpans.push({ off: this.stringOffset, len: bytes.length });
    this.stringData.push(bytes);
    this.stringOffset += bytes.length;
    return index;
  }

  private writeStyle(view: DataView, offset: number, style: Style): void {
    view.setUint32(offset, style.fg, true);
    view.setUint32(offset + 4, style.bg, true);
    view.setUint32(offset + 8, style.attrs, true);
    view.setUint32(offset + 12, 0, true);  // reserved
  }
}

function align4(n: number): number {
  return (n + 3) & ~3;
}
```

### Event Parser

```typescript
function parseEventBatch(buffer: Uint8Array): Event[] {
  const view = new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength);

  // Validate header
  const magic = view.getUint32(0, true);
  if (magic !== 0x5A524556) {
    throw new Error('Invalid event batch magic');
  }

  const eventCount = view.getUint32(12, true);
  const flags = view.getUint32(16, true);
  const truncated = (flags & 1) !== 0;

  const events: Event[] = [];
  let offset = 24;  // After header

  for (let i = 0; i < eventCount; i++) {
    const type = view.getUint32(offset, true);
    const size = view.getUint32(offset + 4, true);
    const timeMs = view.getUint32(offset + 8, true);

    const payloadOffset = offset + 16;

    switch (type) {
      case 1:  // KEY
        events.push({
          type: 'key',
          timeMs,
          key: view.getUint32(payloadOffset, true),
          mods: view.getUint32(payloadOffset + 4, true),
          action: view.getUint32(payloadOffset + 8, true),
        });
        break;
      case 2:  // TEXT
        events.push({
          type: 'text',
          timeMs,
          codepoint: view.getUint32(payloadOffset, true),
        });
        break;
      case 4:  // MOUSE
        events.push({
          type: 'mouse',
          timeMs,
          x: view.getInt32(payloadOffset, true),
          y: view.getInt32(payloadOffset + 4, true),
          kind: view.getUint32(payloadOffset + 8, true),
          mods: view.getUint32(payloadOffset + 12, true),
          buttons: view.getUint32(payloadOffset + 16, true),
          wheelX: view.getInt32(payloadOffset + 20, true),
          wheelY: view.getInt32(payloadOffset + 24, true),
        });
        break;
      case 5:  // RESIZE
        events.push({
          type: 'resize',
          timeMs,
          cols: view.getUint32(payloadOffset, true),
          rows: view.getUint32(payloadOffset + 4, true),
        });
        break;
      // Add other event types as needed
    }

    offset += size;
  }

  return events;
}
```

### Main Loop Example

```typescript
async function main() {
  const engine = new ZiraelEngine({
    colorMode: 'rgb',
    enableMouse: true,
  });

  try {
    let running = true;

    while (running) {
      // Poll events (16ms timeout for ~60fps)
      const events = engine.pollEvents(16);

      for (const event of events) {
        if (event.type === 'key' && event.key === ZrKey.ESCAPE) {
          running = false;
        }
        // Handle other events...
      }

      // Build drawlist
      const dl = new DrawlistBuilder()
        .clear()
        .fillRect(0, 0, 80, 24, { fg: 0xFFFFFF, bg: 0x000000, attrs: 0 })
        .drawText(10, 10, "Hello, Zireael! 👋🏽", {
          fg: 0x00FF00, bg: 0x000000, attrs: ZrAttr.BOLD
        })
        .build();

      // Render
      engine.submitDrawlist(dl);
      engine.present();
    }
  } finally {
    engine.destroy();
  }
}
```

---

## Appendix: Struct Sizes

| Struct | Size (bytes) | Notes |
|--------|--------------|-------|
| `zr_dl_header_t` | 64 | Drawlist header |
| `zr_dl_cmd_header_t` | 8 | Command header |
| `zr_dl_style_t` | 16 | Style struct |
| `zr_dl_span_t` | 8 | String/blob span |
| `zr_evbatch_header_t` | 24 | Event batch header |
| `zr_ev_record_header_t` | 16 | Event record header |
| `zr_ev_key_t` | 16 | Key event payload |
| `zr_ev_text_t` | 8 | Text event payload |
| `zr_ev_mouse_t` | 32 | Mouse event payload |
| `zr_ev_resize_t` | 16 | Resize event payload |
| `zr_cell_t` | 52 | Framebuffer cell (internal) |

---

*Document generated for Zireael v1.0.0 ABI*
