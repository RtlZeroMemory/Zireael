/*
  src/platform/zr_platform_select.c — Backend selection glue for plat_create().

  Why: Centralizes the only non-backend platform preprocessor selection so the
  core can stay OS-header-free and backend-agnostic.
*/

#include "platform/zr_platform.h"

#if defined(_WIN32)
zr_result_t zr_plat_win32_create(plat_t** out_plat, const plat_config_t* cfg);
#else
zr_result_t zr_plat_posix_create(plat_t** out_plat, const plat_config_t* cfg);
#endif

zr_result_t plat_create(plat_t** out_plat, const plat_config_t* cfg) {
  if (!out_plat || !cfg) {
    return ZR_ERR_INVALID_ARGUMENT;
  }
  *out_plat = NULL;
#if defined(_WIN32)
  return zr_plat_win32_create(out_plat, cfg);
#else
  return zr_plat_posix_create(out_plat, cfg);
#endif
}
