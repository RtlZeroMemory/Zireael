/*
  src/platform/posix/zr_plat_posix_stub.c — Temporary POSIX backend stub.

  Why: Keeps the build green until the real POSIX backend lands (EPIC-007).
*/

#include "platform/zr_platform.h"

zr_result_t zr_plat_posix_create(plat_t** out_plat, const plat_config_t* cfg) {
  (void)out_plat;
  (void)cfg;
  return ZR_ERR_UNSUPPORTED;
}

