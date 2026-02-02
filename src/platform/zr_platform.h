/*
  src/platform/zr_platform.h — OS-header-free platform boundary interface.

  Why: Provides a hard boundary between the deterministic core (no OS headers)
  and platform backends (POSIX/Win32). Core code talks only to this interface.
*/

#ifndef ZR_PLATFORM_ZR_PLATFORM_H_INCLUDED
#define ZR_PLATFORM_ZR_PLATFORM_H_INCLUDED

#include "zr/zr_platform_types.h"

#include "util/zr_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Opaque platform handle. Implemented by the platform backend. */
typedef struct plat_t plat_t;

/* lifecycle */
zr_result_t plat_create(plat_t** out_plat, const plat_config_t* cfg);
void        plat_destroy(plat_t* plat);

/* raw mode (idempotent, best-effort) */
zr_result_t plat_enter_raw(plat_t* plat);
zr_result_t plat_leave_raw(plat_t* plat);

/* caps/size */
zr_result_t plat_get_size(plat_t* plat, plat_size_t* out_size);
zr_result_t plat_get_caps(plat_t* plat, plat_caps_t* out_caps);

/* I/O */
int32_t     plat_read_input(plat_t* plat, uint8_t* out_buf, int32_t out_cap);
zr_result_t plat_write_output(plat_t* plat, const uint8_t* bytes, int32_t len);

/*
  Output backpressure:
    - plat_wait_output_writable returns:
        ZR_OK             : output is writable within timeout
        ZR_ERR_LIMIT      : timeout
        ZR_ERR_UNSUPPORTED: backend cannot support this operation
      other negative      : platform failure
*/
zr_result_t plat_wait_output_writable(plat_t* plat, int32_t timeout_ms);

/*
  wait/wake:
    - plat_wait returns:
        1 : woke or input-ready
        0 : timeout
      < 0 : negative ZR_ERR_* failure
    - plat_wake is callable from non-engine threads and must not block indefinitely.
*/
int32_t     plat_wait(plat_t* plat, int32_t timeout_ms);
zr_result_t plat_wake(plat_t* plat);

/* time */
uint64_t plat_now_ms(void);

#endif /* ZR_PLATFORM_ZR_PLATFORM_H_INCLUDED */
