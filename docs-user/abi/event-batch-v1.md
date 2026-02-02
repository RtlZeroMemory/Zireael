# Event Batch v1

The engine writes input events as a packed binary batch into a caller-provided buffer via `engine_poll_events()`.

`engine_poll_events()` only accepts `timeout_ms >= 0`:

- `timeout_ms = 0`: non-blocking
- `timeout_ms > 0`: wait up to N milliseconds

## Layout

```
┌────────────────────────────────────────┐
│ Batch Header (zr_evbatch_header_t)     │
│                              24 bytes  │
├────────────────────────────────────────┤
│ Record 0                               │
│   record_header + payload              │
├────────────────────────────────────────┤
│ Record 1                               │
│   record_header + payload              │
├────────────────────────────────────────┤
│ ...                                    │
└────────────────────────────────────────┘
```

All records are 4-byte aligned.

## Batch Header (24 bytes)

```
Offset  Size  Field        Description
──────  ────  ─────        ───────────
0x00    4     magic        0x5645525A ("ZREV" little-endian)
0x04    4     version      1
0x08    4     total_size   Total batch size in bytes
0x0C    4     event_count  Number of event records
0x10    4     flags        Batch flags (see below)
0x14    4     reserved0    Must be 0
```

Batch flags:

| Flag | Bit | Description |
|------|-----|-------------|
| `ZR_EV_BATCH_TRUNCATED` | 0 | Buffer was too small; some events dropped |

## Record Header (16 bytes)

Each event record starts with:

```
Offset  Size  Field    Description
──────  ────  ─────    ───────────
0x00    4     type     Event type (zr_event_type_t)
0x04    4     size     Total record size including header
0x08    4     time_ms  Timestamp in milliseconds
0x0C    4     flags    Record flags (reserved, must be 0)
```

## Event Types

| Type | Value | Payload Size | Description |
|------|-------|--------------|-------------|
| `ZR_EV_INVALID` | 0 | — | Invalid (should not appear) |
| `ZR_EV_KEY` | 1 | 16 bytes | Key press/release |
| `ZR_EV_TEXT` | 2 | 8 bytes | Text input (Unicode codepoint) |
| `ZR_EV_PASTE` | 3 | 8+ bytes | Paste from clipboard |
| `ZR_EV_MOUSE` | 4 | 32 bytes | Mouse event |
| `ZR_EV_RESIZE` | 5 | 16 bytes | Terminal resize |
| `ZR_EV_TICK` | 6 | 16 bytes | Frame tick |
| `ZR_EV_USER` | 7 | 16+ bytes | User-posted event |

## Event Payloads

### KEY (16 bytes after header)

```
Offset  Size  Field      Description
──────  ────  ─────      ───────────
0x00    4     key        Key code (zr_key_t)
0x04    4     mods       Modifier mask (ZR_MOD_*)
0x08    4     action     Action (zr_key_action_t)
0x0C    4     reserved0  Must be 0
```

Key codes:

| Key | Value |
|-----|-------|
| `ZR_KEY_UNKNOWN` | 0 |
| `ZR_KEY_ESCAPE` | 1 |
| `ZR_KEY_ENTER` | 2 |
| `ZR_KEY_TAB` | 3 |
| `ZR_KEY_BACKSPACE` | 4 |
| `ZR_KEY_INSERT` | 10 |
| `ZR_KEY_DELETE` | 11 |
| `ZR_KEY_HOME` | 12 |
| `ZR_KEY_END` | 13 |
| `ZR_KEY_PAGE_UP` | 14 |
| `ZR_KEY_PAGE_DOWN` | 15 |
| `ZR_KEY_UP` | 20 |
| `ZR_KEY_DOWN` | 21 |
| `ZR_KEY_LEFT` | 22 |
| `ZR_KEY_RIGHT` | 23 |
| `ZR_KEY_F1`–`ZR_KEY_F12` | 100–111 |

Modifier mask:

| Modifier | Bit |
|----------|-----|
| `ZR_MOD_SHIFT` | 0 |
| `ZR_MOD_CTRL` | 1 |
| `ZR_MOD_ALT` | 2 |
| `ZR_MOD_META` | 3 |

Key action:

| Action | Value |
|--------|-------|
| `ZR_KEY_ACTION_INVALID` | 0 |
| `ZR_KEY_ACTION_DOWN` | 1 |
| `ZR_KEY_ACTION_UP` | 2 |
| `ZR_KEY_ACTION_REPEAT` | 3 |

### TEXT (8 bytes after header)

```
Offset  Size  Field      Description
──────  ────  ─────      ───────────
0x00    4     codepoint  Unicode scalar value (U+0000..U+10FFFF)
0x04    4     reserved0  Must be 0
```

### PASTE (8+ bytes after header)

```
Offset  Size  Field      Description
──────  ────  ─────      ───────────
0x00    4     byte_len   Length of UTF-8 data
0x04    4     reserved0  Must be 0
0x08    N     data       UTF-8 bytes (padded to 4-byte alignment)
```

### MOUSE (32 bytes after header)

```
Offset  Size  Field      Description
──────  ────  ─────      ───────────
0x00    4     x          Column position (signed)
0x04    4     y          Row position (signed)
0x08    4     kind       Mouse event kind (zr_mouse_kind_t)
0x0C    4     mods       Modifier mask (ZR_MOD_*)
0x10    4     buttons    Button state bitmask
0x14    4     wheel_x    Horizontal scroll (signed)
0x18    4     wheel_y    Vertical scroll (signed)
0x1C    4     reserved0  Must be 0
```

Mouse kind:

| Kind | Value |
|------|-------|
| `ZR_MOUSE_INVALID` | 0 |
| `ZR_MOUSE_MOVE` | 1 |
| `ZR_MOUSE_DRAG` | 2 |
| `ZR_MOUSE_DOWN` | 3 |
| `ZR_MOUSE_UP` | 4 |
| `ZR_MOUSE_WHEEL` | 5 |

### RESIZE (16 bytes after header)

```
Offset  Size  Field      Description
──────  ────  ─────      ───────────
0x00    4     cols       New column count
0x04    4     rows       New row count
0x08    4     reserved0  Must be 0
0x0C    4     reserved1  Must be 0
```

### TICK (16 bytes after header)

```
Offset  Size  Field      Description
──────  ────  ─────      ───────────
0x00    4     dt_ms      Milliseconds since last tick
0x04    4     reserved0  Must be 0
0x08    4     reserved1  Must be 0
0x0C    4     reserved2  Must be 0
```

### USER (16+ bytes after header)

```
Offset  Size  Field      Description
──────  ────  ─────      ───────────
0x00    4     tag        User-defined tag
0x04    4     byte_len   Payload length
0x08    4     reserved0  Must be 0
0x0C    4     reserved1  Must be 0
0x10    N     data       Opaque payload (padded to 4-byte alignment)
```

## Parsing Example (C)

Casting `uint8_t*` to structs can be undefined behavior if the buffer pointer is not sufficiently aligned. The safest approach is to parse using explicit little-endian reads.

```c
static uint32_t rd_u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) | ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

void parse_events(const uint8_t* buf, int len) {
    if (!buf || len < 24) return;
    const uint32_t magic = rd_u32le(buf + 0);
    const uint32_t version = rd_u32le(buf + 4);
    const uint32_t total = rd_u32le(buf + 8);
    const uint32_t count = rd_u32le(buf + 12);
    const uint32_t flags = rd_u32le(buf + 16);

    if (magic != ZR_EV_MAGIC || version != 1) return;
    if (total > (uint32_t)len) return;

    const uint8_t* ptr = buf + 24;
    const uint8_t* end = buf + total;

    for (uint32_t i = 0; i < count && ptr + 16 <= end; i++) {
        const uint32_t type = rd_u32le(ptr + 0);
        const uint32_t size = rd_u32le(ptr + 4);
        if ((size & 3u) != 0u || size < 16u || ptr + size > end) break;

        const uint8_t* payload = ptr + 16;
        switch (type) {
        case ZR_EV_KEY: {
            if (size < 16u + 16u) break;
            uint32_t key = rd_u32le(payload + 0);
            uint32_t mods = rd_u32le(payload + 4);
            uint32_t action = rd_u32le(payload + 8);
            (void)key; (void)mods; (void)action;
            break;
        }
        case ZR_EV_RESIZE: {
            if (size < 16u + 16u) break;
            uint32_t cols = rd_u32le(payload + 0);
            uint32_t rows = rd_u32le(payload + 4);
            (void)cols; (void)rows;
            break;
        }
        default:
            break; /* forward-compatible skip */
        }

        ptr += size;
    }

    if (flags & ZR_EV_BATCH_TRUNCATED) {
        /* some events were dropped; consider a larger buffer */
    }
}
```

## Truncation

If the caller buffer cannot fit all events:

1. Engine writes as many complete records as fit
2. Sets `ZR_EV_BATCH_TRUNCATED` flag
3. Returns bytes written (success)

The wrapper should use a larger buffer if truncation occurs frequently.

## Forward Compatibility

Unknown record types can be skipped using `record_header.size`. This allows older wrappers to work with newer engines that emit new event types.
