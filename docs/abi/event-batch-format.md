# Event Batch Format

`engine_poll_events()` writes a packed, versioned event batch into a caller-provided byte buffer.

Authoritative implementation spec:

- [Internal Specs -> Event System and Packed Event ABI](../modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md)

## High-Level Layout

```text
+--------------------------------+
| zr_evbatch_header_t (24 bytes) |
+--------------------------------+
| record 0                        |
+--------------------------------+
| record 1                        |
+--------------------------------+
| ...                             |
+--------------------------------+
```

All fields are little-endian.

## Batch Header (`zr_evbatch_header_t`)

Key fields:

- `magic == 0x5645525A` (`'ZREV'` little-endian)
- `version == ZR_EVENT_BATCH_VERSION_V1`
- `total_size`: total emitted bytes
- `event_count`: number of complete records emitted
- `flags`: batch-level flags (`ZR_EV_BATCH_TRUNCATED` etc.)

## Record Framing

Every record starts with `zr_ev_record_header_t`:

- `type`
- `size` (total record bytes, header + payload)
- `time_ms`
- `flags`

Records are 4-byte aligned. Wrappers should advance by `align4(size)`.

## Event Types (v1)

| Type | Value | Payload |
|---|---:|---|
| `ZR_EV_KEY` | 1 | `zr_ev_key_t` |
| `ZR_EV_TEXT` | 2 | `zr_ev_text_t` |
| `ZR_EV_PASTE` | 3 | `zr_ev_paste_t + utf8 bytes + pad` |
| `ZR_EV_MOUSE` | 4 | `zr_ev_mouse_t` |
| `ZR_EV_RESIZE` | 5 | `zr_ev_resize_t` |
| `ZR_EV_TICK` | 6 | `zr_ev_tick_t` |
| `ZR_EV_USER` | 7 | `zr_ev_user_t + payload bytes + pad` |

## Truncation Semantics

If output capacity cannot fit all queued records:

- call still succeeds
- only complete records are emitted
- batch `flags` contains `ZR_EV_BATCH_TRUNCATED`

This is not a parse error. It is a capacity signal.

## Forward Compatibility

Wrappers must:

- validate record bounds before reading payload
- skip unknown `type` values by `size`
- avoid assuming contiguous known-type-only streams

## Parsing Skeleton

```c
const zr_evbatch_header_t* hdr = (const zr_evbatch_header_t*)buf;
uint32_t off = (uint32_t)sizeof(*hdr);

while (off + (uint32_t)sizeof(zr_ev_record_header_t) <= hdr->total_size) {
  const zr_ev_record_header_t* rec = (const zr_ev_record_header_t*)(buf + off);
  if (rec->size < sizeof(*rec) || off + rec->size > hdr->total_size) {
    break; /* malformed */
  }

  const uint8_t* payload = buf + off + (uint32_t)sizeof(*rec);
  /* switch(rec->type) ... */

  off += (rec->size + 3u) & ~3u;
}
```

## Wrapper Buffer Sizing Guidance

- Start with at least 4 KiB event buffers.
- Increase when paste-heavy or mouse-heavy sessions set `ZR_EV_BATCH_TRUNCATED` often.
- Monitor truncation frequency and treat repeated truncation as backpressure.

## Next Steps

- [C ABI Reference](c-abi-reference.md)
- [Input Model](../user-guide/input-model.md)
- [Internal Event ABI Spec](../modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md)
