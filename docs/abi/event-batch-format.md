# Event batch format

The engine returns input events as a packed, versioned **event batch** byte stream.

This page is a wrapper-facing guide. The implementation-ready spec is:

- [Internal Specs → Event System and Packed Event ABI](../modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md)

## Quick rules

- Little-endian.
- Batch begins with `zr_evbatch_header_t` (magic/version/total_size/event_count/flags).
- Each record is `{type, size, payload}` framed by `zr_ev_record_header_t.size` (skippable by size).
- Unknown record types must be skipped, not treated as errors (forward-compat).
- Records are 4-byte aligned; wrappers should advance by `align4(size)`.

## Truncation

If the caller buffer cannot fit all records, `engine_poll_events()` succeeds with a truncated batch:

- `zr_evbatch_header_t.flags` includes `ZR_EV_BATCH_TRUNCATED`
- only complete records are emitted

Wrappers should treat truncation as a signal to increase their event buffer size if it happens frequently.

## Paste events (bracketed paste)

`ZR_EV_PASTE` records represent bracketed paste payloads as UTF-8 bytes:

- payload begins with `zr_ev_paste_t { u32 byte_len; ... }`
- followed by `byte_len` bytes of UTF-8
- followed by zero padding to 4-byte alignment

Wrappers should validate `byte_len` against the record size before reading.

## Text events

`ZR_EV_TEXT` records carry one Unicode scalar value per event (`zr_ev_text_t.codepoint`).
Input bytes are decoded as UTF-8; malformed byte sequences are normalized to `U+FFFD`.

## Parsing sketch

```c
/* Pseudocode (bounds checks omitted): */
hdr = (zr_evbatch_header_t*)buf;
off = sizeof(*hdr);
while (off + sizeof(zr_ev_record_header_t) <= hdr->total_size) {
  rec = (zr_ev_record_header_t*)(buf + off);
  payload = buf + off + sizeof(*rec);
  /* switch(rec->type) ... */
  off += align4(rec->size);
}
```

## Next steps

- [C ABI Reference](c-abi-reference.md)
- [Internal Specs → Events](../modules/EVENT_SYSTEM_AND_PACKED_EVENT_ABI.md)
