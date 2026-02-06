/*
  src/util/zr_log.h — Log sink indirection.

  Why: Allows the engine to emit log messages without stdio/printf and without
  owning I/O resources. If no sink is set, logging is a no-op.
  Sink install/write paths are synchronized for cross-thread safety.
*/

#ifndef ZR_UTIL_ZR_LOG_H_INCLUDED
#define ZR_UTIL_ZR_LOG_H_INCLUDED

#include "util/zr_string_view.h"

typedef void (*zr_log_sink_fn_t)(void* user, zr_string_view_t msg);

void zr_log_set_sink(zr_log_sink_fn_t sink, void* user);
void zr_log_write(zr_string_view_t msg);

#endif /* ZR_UTIL_ZR_LOG_H_INCLUDED */
