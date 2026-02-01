/*
  src/core/zr_config.h — Public engine configuration and validation surface.

  Why: Defines the stable config structs used for engine creation and
  runtime reconfiguration, keeping ownership and platform boundaries explicit.
*/

#ifndef ZR_CORE_ZR_CONFIG_H_INCLUDED
#define ZR_CORE_ZR_CONFIG_H_INCLUDED

#include "core/zr_version.h"

#include "platform/zr_platform.h"

#include "util/zr_caps.h"
#include "util/zr_result.h"

#include <stdint.h>

/*
  zr_engine_config_t:
    - Passed to engine_create() for ABI negotiation and initial setup.
    - Ownership: the engine does not retain pointers into this struct; it may
      copy the values it needs.
*/
typedef struct zr_engine_config_t {
  /* --- Version negotiation (pinned; checked at engine_create) --- */
  uint32_t requested_engine_abi_major;
  uint32_t requested_engine_abi_minor;
  uint32_t requested_engine_abi_patch;

  uint32_t requested_drawlist_version;     /* e.g. ZR_DRAWLIST_VERSION_V1 */
  uint32_t requested_event_batch_version;  /* e.g. ZR_EVENT_BATCH_VERSION_V1 */

  /* --- Deterministic caps --- */
  zr_limits_t limits;

  /* --- Platform policy (OS-header-free boundary) --- */
  plat_config_t plat;

  /* --- Text policy --- */
  uint32_t tab_width;
  uint32_t width_policy; /* zr_width_policy_t values (see src/unicode/zr_width.h) */

  /* --- Scheduling --- */
  uint32_t target_fps;

  /* --- Feature toggles (0/1 for ABI stability) --- */
  uint8_t enable_scroll_optimizations;
  uint8_t enable_debug_overlay;
  uint8_t enable_replay_recording;
  uint8_t _pad0; /* must be 0 in v1 */
} zr_engine_config_t;

/*
  zr_engine_runtime_config_t:
    - Passed to engine_set_config() to adjust runtime behavior.
    - Ownership: the engine does not retain pointers into this struct; it may
      copy the values it needs.
*/
typedef struct zr_engine_runtime_config_t {
  zr_limits_t limits;
  plat_config_t plat;

  uint32_t tab_width;
  uint32_t width_policy; /* zr_width_policy_t values (see src/unicode/zr_width.h) */
  uint32_t target_fps;

  uint8_t enable_scroll_optimizations;
  uint8_t enable_debug_overlay;
  uint8_t enable_replay_recording;
  uint8_t _pad0; /* must be 0 in v1 */
} zr_engine_runtime_config_t;

zr_engine_config_t zr_engine_config_default(void);
zr_result_t zr_engine_config_validate(const zr_engine_config_t* cfg);
zr_result_t zr_engine_runtime_config_validate(const zr_engine_runtime_config_t* cfg);

#endif /* ZR_CORE_ZR_CONFIG_H_INCLUDED */
