/*
  src/core/zr_detect.h — Startup terminal capability detection and parsing.

  Why: Centralizes query-batch construction, response parsing, profile shaping,
  and override application for deterministic engine_create() detection.
*/

#ifndef ZR_CORE_ZR_DETECT_H_INCLUDED
#define ZR_CORE_ZR_DETECT_H_INCLUDED

#include "platform/zr_platform.h"

#include "zr/zr_terminal_caps.h"

#include <stddef.h>
#include <stdint.h>

typedef struct zr_detect_parsed_t {
  char xtversion_raw[64];
  zr_terminal_id_t xtversion_id;
  uint8_t xtversion_responded;

  uint8_t da1_responded;
  uint8_t da1_has_sixel;
  uint8_t da2_responded;
  uint8_t _pad0;

  uint32_t da2_model;
  uint32_t da2_version;

  uint8_t decrqm_2026_seen;
  uint8_t decrqm_2026_value;
  uint8_t decrqm_2027_seen;
  uint8_t decrqm_2027_value;
  uint8_t decrqm_1016_seen;
  uint8_t decrqm_1016_value;
  uint8_t decrqm_2004_seen;
  uint8_t decrqm_2004_value;

  uint16_t cell_width_px;
  uint16_t cell_height_px;
  uint16_t screen_width_px;
  uint16_t screen_height_px;
} zr_detect_parsed_t;

/* Return immutable startup query batch bytes (XTVERSION/DA/DECRQM/cell metrics). */
const uint8_t* zr_detect_query_batch_bytes(size_t* out_len);

/* Reset parsed response state to deterministic defaults. */
void zr_detect_parsed_reset(zr_detect_parsed_t* out_parsed);

/* Parse zero or more probe responses from an arbitrary byte stream. */
zr_result_t zr_detect_parse_responses(const uint8_t* bytes, size_t len, zr_detect_parsed_t* io_parsed);

/*
  Probe terminal capabilities at startup.

  Returns:
    - ZR_OK on success
    - ZR_ERR_INVALID_ARGUMENT on invalid pointers
    - ZR_ERR_PLATFORM when write/read probing fails
    - ZR_ERR_UNSUPPORTED when probing is unavailable for this platform mode

  Optional passthrough outputs capture bytes consumed during probing that are not
  recognized as probe replies, so startup user input can be re-queued.
*/
zr_result_t zr_detect_probe_terminal(plat_t* plat, const plat_caps_t* baseline_caps, zr_terminal_profile_t* out_profile,
                                     plat_caps_t* out_caps, uint8_t* out_passthrough, size_t passthrough_cap,
                                     size_t* out_passthrough_len);

/*
  Apply force/suppress override flags to a base profile/caps snapshot.

  Precedence: suppress wins over force for overlapping bits.
*/
void zr_detect_apply_overrides(const zr_terminal_profile_t* base_profile, const plat_caps_t* base_caps,
                               zr_terminal_cap_flags_t force_flags, zr_terminal_cap_flags_t suppress_flags,
                               zr_terminal_profile_t* out_profile, plat_caps_t* out_caps);

/* Convert profile + caps booleans into a unified override flag mask. */
zr_terminal_cap_flags_t zr_detect_profile_cap_flags(const zr_terminal_profile_t* profile, const plat_caps_t* caps);

#endif /* ZR_CORE_ZR_DETECT_H_INCLUDED */
