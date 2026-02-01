/*
  src/platform/win32/zr_plat_win32_stub.c — Temporary Win32 backend stub.

  Why: Keeps the build green until the real Win32 backend lands (EPIC-008).
*/

#include "platform/zr_platform.h"

zr_result_t zr_plat_win32_create(plat_t** out_plat, const plat_config_t* cfg) {
  (void)out_plat;
  (void)cfg;
  return ZR_ERR_UNSUPPORTED;
}

