/*
  src/core/zr_debug_trace.c — Debug trace ring buffer implementation.

  Why: Implements a deterministic, bounded trace buffer for diagnostic records.
  The ring buffer overwrites oldest records when full, ensuring bounded memory
  usage while preserving recent history for debugging.

  Design:
    - Two-ring architecture: byte ring for payloads, index ring for offsets.
    - Variable-length records stored contiguously in byte ring.
    - Index ring provides O(1) lookup by slot without scanning payloads.
    - No allocations after init; all storage is caller-provided.
*/

#include "core/zr_debug_trace.h"

#include <string.h>

/* Category mask bit for a given category. */
static uint32_t zr_debug_cat_bit(zr_debug_category_t cat) {
  if (cat == ZR_DEBUG_CAT_NONE || cat > 31) {
    return 0u;
  }
  return 1u << (uint32_t)cat;
}

/* Compute relative timestamp in microseconds from engine start. */
static uint64_t zr_debug_relative_timestamp_us(const zr_debug_trace_t* t, uint64_t absolute_us) {
  if (!t || absolute_us < t->start_time_us) {
    return 0u;
  }
  return absolute_us - t->start_time_us;
}

/* Saturating conversion from u64 counters to ABI-facing u32 fields. */
static uint32_t zr_debug_u64_to_u32_sat(uint64_t v) {
  if (v > (uint64_t)UINT32_MAX) {
    return UINT32_MAX;
  }
  return (uint32_t)v;
}

/*
  Evict oldest records to make room for a new record.

  Why: Ring buffer semantics require overwriting oldest data when full.
  We evict whole records (not partial) to maintain index consistency.
*/
static void zr_debug_trace_evict(zr_debug_trace_t* t, size_t needed_bytes) {
  if (!t || t->index_count == 0u) {
    return;
  }

  /*
    Ensure there is an index slot available for the incoming record.

    Why: The byte ring can be large enough to hold more records than the index
    ring. When the index ring is full, we must evict exactly one oldest record
    so index and byte eviction remain consistent.
  */
  if (t->index_count == t->index_cap) {
    const uint32_t tail_slot = t->index_head;
    const uint32_t record_size = t->record_sizes[tail_slot];
    if (record_size <= t->byte_used) {
      t->byte_used -= record_size;
    } else {
      t->byte_used = 0u;
    }
    t->index_count--;
    t->total_dropped++;
  }

  while (t->byte_used + needed_bytes > t->ring_buf_cap && t->index_count > 0u) {
    /* Find oldest record (tail of index ring). */
    uint32_t tail_slot = (t->index_head + t->index_cap - t->index_count) % t->index_cap;
    uint32_t record_size = t->record_sizes[tail_slot];

    t->byte_used -= record_size;
    t->index_count--;
    t->total_dropped++;
  }
}

zr_debug_config_t zr_debug_config_default(void) {
  zr_debug_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.enabled = 0u; /* Disabled by default for performance. */
  cfg.ring_capacity = ZR_DEBUG_DEFAULT_RING_CAP;
  cfg.min_severity = ZR_DEBUG_SEV_INFO;
  /* Enable all categories by default. */
  cfg.category_mask = 0xFFFFFFFFu;
  cfg.capture_raw_events = 0u;
  cfg.capture_drawlist_bytes = 0u;
  return cfg;
}

zr_result_t zr_debug_trace_init(zr_debug_trace_t* t, const zr_debug_config_t* config, uint8_t* ring_buf,
                                size_t ring_buf_cap, uint32_t* record_offsets, uint32_t* record_sizes,
                                uint32_t index_cap) {
  if (!t) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  memset(t, 0, sizeof(*t));

  if (!config) {
    t->config = zr_debug_config_default();
  } else {
    t->config = *config;
  }

  if (!t->config.enabled) {
    /* Tracing disabled; storage not required. */
    return ZR_OK;
  }

  if (!ring_buf || ring_buf_cap == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  if (!record_offsets || !record_sizes || index_cap == 0u) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  t->ring_buf = ring_buf;
  t->ring_buf_cap = ring_buf_cap;
  t->record_offsets = record_offsets;
  t->record_sizes = record_sizes;
  t->index_cap = index_cap;

  t->index_head = 0u;
  t->index_count = 0u;
  t->next_record_id = 1u;
  t->total_dropped = 0u;
  t->current_frame_id = 0u;
  t->start_time_us = 0u;
  t->error_count = 0u;
  t->warn_count = 0u;
  t->byte_head = 0u;
  t->byte_used = 0u;

  return ZR_OK;
}

void zr_debug_trace_reset(zr_debug_trace_t* t) {
  if (!t) {
    return;
  }

  t->index_head = 0u;
  t->index_count = 0u;
  t->next_record_id = 1u;
  t->total_dropped = 0u;
  t->error_count = 0u;
  t->warn_count = 0u;
  t->byte_head = 0u;
  t->byte_used = 0u;
}

void zr_debug_trace_set_frame(zr_debug_trace_t* t, uint64_t frame_id) {
  if (t) {
    t->current_frame_id = frame_id;
  }
}

void zr_debug_trace_set_start_time(zr_debug_trace_t* t, uint64_t start_time_us) {
  if (t) {
    t->start_time_us = start_time_us;
  }
}

bool zr_debug_trace_enabled(const zr_debug_trace_t* t, zr_debug_category_t category, zr_debug_severity_t severity) {
  if (!t || !t->config.enabled) {
    return false;
  }
  if ((uint32_t)severity < t->config.min_severity) {
    return false;
  }
  if ((t->config.category_mask & zr_debug_cat_bit(category)) == 0u) {
    return false;
  }
  return true;
}

zr_result_t zr_debug_trace_record(zr_debug_trace_t* t, zr_debug_category_t category, zr_debug_severity_t severity,
                                  uint32_t code, uint64_t timestamp_us, const void* payload, uint32_t payload_size) {
  if (!t) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  /* Early exit if tracing disabled or filtered. */
  if (!zr_debug_trace_enabled(t, category, severity)) {
    return ZR_OK;
  }

  if (payload_size > ZR_DEBUG_MAX_PAYLOAD_SIZE) {
    return ZR_ERR_LIMIT;
  }
  if (payload_size > 0u && !payload) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  /* Check storage was provided. */
  if (!t->ring_buf || !t->record_offsets || !t->record_sizes) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  const size_t header_size = sizeof(zr_debug_record_header_t);
  const size_t total_size = header_size + (size_t)payload_size;

  if (total_size > t->ring_buf_cap) {
    return ZR_ERR_LIMIT;
  }

  /* Evict old records if needed. */
  zr_debug_trace_evict(t, total_size);

  /* Build header with relative timestamp. */
  zr_debug_record_header_t hdr;
  memset(&hdr, 0, sizeof(hdr));
  hdr.record_id = t->next_record_id++;
  hdr.timestamp_us = zr_debug_relative_timestamp_us(t, timestamp_us);
  hdr.frame_id = t->current_frame_id;
  hdr.category = (uint32_t)category;
  hdr.severity = (uint32_t)severity;
  hdr.code = code;
  hdr.payload_size = payload_size;

  /* Write header to byte ring. */
  size_t write_pos = t->byte_head;
  if (write_pos + header_size <= t->ring_buf_cap) {
    memcpy(t->ring_buf + write_pos, &hdr, header_size);
  } else {
    /* Wrap around (shouldn't happen if eviction is correct, but handle it). */
    size_t first_part = t->ring_buf_cap - write_pos;
    memcpy(t->ring_buf + write_pos, &hdr, first_part);
    memcpy(t->ring_buf, ((const uint8_t*)&hdr) + first_part, header_size - first_part);
  }

  /* Write payload to byte ring. */
  if (payload_size > 0u) {
    size_t payload_pos = (write_pos + header_size) % t->ring_buf_cap;
    if (payload_pos + payload_size <= t->ring_buf_cap) {
      memcpy(t->ring_buf + payload_pos, payload, payload_size);
    } else {
      size_t first_part = t->ring_buf_cap - payload_pos;
      memcpy(t->ring_buf + payload_pos, payload, first_part);
      memcpy(t->ring_buf, ((const uint8_t*)payload) + first_part, payload_size - first_part);
    }
  }

  /* Update index ring. */
  t->record_offsets[t->index_head] = (uint32_t)write_pos;
  t->record_sizes[t->index_head] = (uint32_t)total_size;

  t->index_head = (t->index_head + 1u) % t->index_cap;
  if (t->index_count < t->index_cap) {
    t->index_count++;
  }

  t->byte_head = (t->byte_head + total_size) % t->ring_buf_cap;
  t->byte_used += total_size;

  /* Update aggregate counters. */
  if (severity == ZR_DEBUG_SEV_ERROR) {
    t->error_count++;
  } else if (severity == ZR_DEBUG_SEV_WARN) {
    t->warn_count++;
  }

  return ZR_OK;
}

zr_result_t zr_debug_trace_frame(zr_debug_trace_t* t, uint32_t code, uint64_t timestamp_us,
                                 const zr_debug_frame_record_t* frame) {
  if (!frame) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_debug_trace_record(t, ZR_DEBUG_CAT_FRAME, ZR_DEBUG_SEV_INFO, code, timestamp_us, frame, sizeof(*frame));
}

zr_result_t zr_debug_trace_event(zr_debug_trace_t* t, uint32_t code, zr_debug_severity_t severity,
                                 uint64_t timestamp_us, const zr_debug_event_record_t* event) {
  if (!event) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_debug_trace_record(t, ZR_DEBUG_CAT_EVENT, severity, code, timestamp_us, event, sizeof(*event));
}

zr_result_t zr_debug_trace_error(zr_debug_trace_t* t, uint32_t code, uint64_t timestamp_us,
                                 const zr_debug_error_record_t* error) {
  if (!error) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_debug_trace_record(t, ZR_DEBUG_CAT_ERROR, ZR_DEBUG_SEV_ERROR, code, timestamp_us, error, sizeof(*error));
}

zr_result_t zr_debug_trace_drawlist(zr_debug_trace_t* t, uint32_t code, uint64_t timestamp_us,
                                    const zr_debug_drawlist_record_t* dl) {
  if (!dl) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_debug_trace_record(t, ZR_DEBUG_CAT_DRAWLIST, ZR_DEBUG_SEV_INFO, code, timestamp_us, dl, sizeof(*dl));
}

zr_result_t zr_debug_trace_perf(zr_debug_trace_t* t, uint64_t timestamp_us, const zr_debug_perf_record_t* perf) {
  if (!perf) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  return zr_debug_trace_record(t, ZR_DEBUG_CAT_PERF, ZR_DEBUG_SEV_TRACE, ZR_DEBUG_CODE_PERF_TIMING, timestamp_us, perf,
                               sizeof(*perf));
}

/*
  Find index slot containing record_id, if still in buffer.

  Returns true and sets out_slot if found.
*/
static bool zr_debug_find_record_slot(const zr_debug_trace_t* t, uint64_t record_id, uint32_t* out_slot) {
  if (!t || t->index_count == 0u || !out_slot) {
    return false;
  }

  /* Records are stored newest at index_head-1, oldest at tail. */
  for (uint32_t i = 0u; i < t->index_count; i++) {
    uint32_t slot = (t->index_head + t->index_cap - 1u - i) % t->index_cap;
    uint32_t offset = t->record_offsets[slot];

    /* Read header from byte ring to get record_id. */
    zr_debug_record_header_t hdr;
    if (offset + sizeof(hdr) <= t->ring_buf_cap) {
      memcpy(&hdr, t->ring_buf + offset, sizeof(hdr));
    } else {
      size_t first_part = t->ring_buf_cap - offset;
      memcpy(&hdr, t->ring_buf + offset, first_part);
      memcpy(((uint8_t*)&hdr) + first_part, t->ring_buf, sizeof(hdr) - first_part);
    }

    if (hdr.record_id == record_id) {
      *out_slot = slot;
      return true;
    }

    /* Records are monotonically increasing; if we've gone past, stop. */
    if (hdr.record_id < record_id) {
      break;
    }
  }

  return false;
}

zr_result_t zr_debug_trace_query(const zr_debug_trace_t* t, const zr_debug_query_t* query,
                                 zr_debug_record_header_t* out_headers, uint32_t out_headers_cap,
                                 zr_debug_query_result_t* out_result) {
  if (!t || !query || !out_result) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  memset(out_result, 0, sizeof(*out_result));

  if (!t->config.enabled || t->index_count == 0u) {
    return ZR_OK;
  }

  uint32_t returned = 0u;
  uint32_t available = 0u;
  uint64_t oldest_id = UINT64_MAX;
  uint64_t newest_id = 0u;

  const uint32_t max_to_return = (query->max_records > 0u) ? query->max_records : UINT32_MAX;

  /* Iterate from newest to oldest. */
  for (uint32_t i = 0u; i < t->index_count; i++) {
    uint32_t slot = (t->index_head + t->index_cap - 1u - i) % t->index_cap;
    uint32_t offset = t->record_offsets[slot];

    /* Read header. */
    zr_debug_record_header_t hdr;
    if (offset + sizeof(hdr) <= t->ring_buf_cap) {
      memcpy(&hdr, t->ring_buf + offset, sizeof(hdr));
    } else {
      size_t first_part = t->ring_buf_cap - offset;
      memcpy(&hdr, t->ring_buf + offset, first_part);
      memcpy(((uint8_t*)&hdr) + first_part, t->ring_buf, sizeof(hdr) - first_part);
    }

    /* Track oldest/newest. */
    if (hdr.record_id < oldest_id) {
      oldest_id = hdr.record_id;
    }
    if (hdr.record_id > newest_id) {
      newest_id = hdr.record_id;
    }

    /* Apply filters. */
    if (query->min_record_id > 0u && hdr.record_id < query->min_record_id) {
      continue;
    }
    if (query->max_record_id > 0u && hdr.record_id > query->max_record_id) {
      continue;
    }
    if (query->min_frame_id > 0u && hdr.frame_id < query->min_frame_id) {
      continue;
    }
    if (query->max_frame_id > 0u && hdr.frame_id > query->max_frame_id) {
      continue;
    }
    if (query->category_mask != 0u &&
        (query->category_mask & zr_debug_cat_bit((zr_debug_category_t)hdr.category)) == 0u) {
      continue;
    }
    if (hdr.severity < query->min_severity) {
      continue;
    }

    available++;

    if (returned < max_to_return && out_headers && returned < out_headers_cap) {
      out_headers[returned] = hdr;
      returned++;
    }
  }

  out_result->records_returned = returned;
  out_result->records_available = available;
  out_result->oldest_record_id = (oldest_id != UINT64_MAX) ? oldest_id : 0u;
  out_result->newest_record_id = newest_id;
  out_result->records_dropped = zr_debug_u64_to_u32_sat(t->total_dropped);

  return ZR_OK;
}

zr_result_t zr_debug_trace_get_payload(const zr_debug_trace_t* t, uint64_t record_id, void* out_payload,
                                       uint32_t out_cap, uint32_t* out_size) {
  if (!t || !out_size) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  *out_size = 0u;

  if (!t->config.enabled) {
    return ZR_ERR_LIMIT;
  }

  uint32_t slot;
  if (!zr_debug_find_record_slot(t, record_id, &slot)) {
    return ZR_ERR_LIMIT;
  }

  uint32_t offset = t->record_offsets[slot];

  /* Read header to get payload size. */
  zr_debug_record_header_t hdr;
  if (offset + sizeof(hdr) <= t->ring_buf_cap) {
    memcpy(&hdr, t->ring_buf + offset, sizeof(hdr));
  } else {
    size_t first_part = t->ring_buf_cap - offset;
    memcpy(&hdr, t->ring_buf + offset, first_part);
    memcpy(((uint8_t*)&hdr) + first_part, t->ring_buf, sizeof(hdr) - first_part);
  }

  *out_size = hdr.payload_size;

  if (hdr.payload_size == 0u) {
    return ZR_OK;
  }

  if (!out_payload || out_cap < hdr.payload_size) {
    return ZR_ERR_LIMIT;
  }

  /* Read payload. */
  size_t payload_offset = (offset + sizeof(hdr)) % t->ring_buf_cap;
  if (payload_offset + hdr.payload_size <= t->ring_buf_cap) {
    memcpy(out_payload, t->ring_buf + payload_offset, hdr.payload_size);
  } else {
    size_t first_part = t->ring_buf_cap - payload_offset;
    memcpy(out_payload, t->ring_buf + payload_offset, first_part);
    memcpy((uint8_t*)out_payload + first_part, t->ring_buf, hdr.payload_size - first_part);
  }

  return ZR_OK;
}

zr_result_t zr_debug_trace_get_stats(const zr_debug_trace_t* t, zr_debug_stats_t* out_stats) {
  if (!t || !out_stats) {
    return ZR_ERR_INVALID_ARGUMENT;
  }

  memset(out_stats, 0, sizeof(*out_stats));

  if (!t->config.enabled) {
    return ZR_OK;
  }

  out_stats->total_records = t->next_record_id - 1u;
  out_stats->total_dropped = t->total_dropped;
  out_stats->error_count = t->error_count;
  out_stats->warn_count = t->warn_count;
  out_stats->current_ring_usage = t->index_count;
  out_stats->ring_capacity = t->index_cap;

  return ZR_OK;
}

int32_t zr_debug_trace_export(const zr_debug_trace_t* t, uint8_t* out_buf, size_t out_cap) {
  if (!t) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }

  if (!t->config.enabled || t->index_count == 0u) {
    return 0;
  }

  if (!out_buf || out_cap == 0u) {
    return (int32_t)ZR_ERR_INVALID_ARGUMENT;
  }

  size_t written = 0u;

  /* Export from oldest to newest. */
  for (uint32_t i = 0u; i < t->index_count; i++) {
    uint32_t slot = (t->index_head + t->index_cap - t->index_count + i) % t->index_cap;
    uint32_t offset = t->record_offsets[slot];
    uint32_t size = t->record_sizes[slot];

    if (written + size > out_cap) {
      break;
    }

    /* Copy record (header + payload) to output. */
    if (offset + size <= t->ring_buf_cap) {
      memcpy(out_buf + written, t->ring_buf + offset, size);
    } else {
      size_t first_part = t->ring_buf_cap - offset;
      memcpy(out_buf + written, t->ring_buf + offset, first_part);
      memcpy(out_buf + written + first_part, t->ring_buf, size - first_part);
    }

    written += size;
  }

  if (written > (size_t)INT32_MAX) {
    return (int32_t)ZR_ERR_LIMIT;
  }

  return (int32_t)written;
}
