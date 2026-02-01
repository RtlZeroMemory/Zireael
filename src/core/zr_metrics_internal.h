/*
  src/core/zr_metrics_internal.h — Internal metrics snapshot plumbing.

  Why: Provides a small core-internal API for updating and copying metrics
  without allocations or global mutable state.
*/

#ifndef ZR_CORE_ZR_METRICS_INTERNAL_H_INCLUDED
#define ZR_CORE_ZR_METRICS_INTERNAL_H_INCLUDED

#include "core/zr_metrics.h"

#include "util/zr_result.h"

/*
  zr_metrics__default_snapshot:
    - Produces a deterministic default snapshot used by early stubs and tests.
    - Pins negotiated versions to src/core/zr_version.h.
*/
zr_metrics_t zr_metrics__default_snapshot(void);

/*
  zr_metrics__copy_out:
    - Reads out_metrics->struct_size and prefix-copies from snapshot into out_metrics.
    - If struct_size is 0, performs a 0-byte copy (no-op).
    - Returns ZR_OK on success; never allocates.
*/
zr_result_t zr_metrics__copy_out(zr_metrics_t* out_metrics, const zr_metrics_t* snapshot);

#endif /* ZR_CORE_ZR_METRICS_INTERNAL_H_INCLUDED */
