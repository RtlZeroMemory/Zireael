/*
  src/util/zr_log.c — Log sink indirection implementation.

  Why: Provides a tiny, deterministic logging hook without any stdio dependency.
*/

#include "util/zr_log.h"

static zr_log_sink_fn_t g_sink = 0;
static void* g_sink_user = 0;

void zr_log_set_sink(zr_log_sink_fn_t sink, void* user) {
  g_sink = sink;
  g_sink_user = user;
}

void zr_log_write(zr_string_view_t msg) {
  if (g_sink) {
    g_sink(g_sink_user, msg);
  }
}

