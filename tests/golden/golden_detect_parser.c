/*
  tests/golden/golden_detect_parser.c — Golden fixtures for detection byte sets.

  Why: Locks representative probe-response byte streams and verifies parser
  behavior remains deterministic across refactors.
*/

#include "zr_test.h"

#include "core/zr_detect.h"

#include "golden/zr_golden.h"

static zr_detect_parsed_t zr_golden_parse_bytes(const uint8_t* bytes, size_t len) {
  zr_detect_parsed_t parsed;
  zr_detect_parsed_reset(&parsed);
  (void)zr_detect_parse_responses(bytes, len, &parsed);
  return parsed;
}

ZR_TEST_GOLDEN(detect_fixture_kitty_full_response_set) {
  static const uint8_t kBytes[] = "\x1bP>|kitty(0.35.0)\x1b\\"
                                  "\x1b[?1;2;22c"
                                  "\x1b[>1;3500;0c"
                                  "\x1b[?2026;1$y"
                                  "\x1b[?2027;1$y"
                                  "\x1b[?1016;1$y"
                                  "\x1b[?2004;1$y"
                                  "\x1b[6;20;10t";

  ZR_ASSERT_EQ_U32((uint32_t)zr_golden_compare_fixture("detect_kitty_full", kBytes, sizeof(kBytes) - 1u), 0u);
  const zr_detect_parsed_t parsed = zr_golden_parse_bytes(kBytes, sizeof(kBytes) - 1u);
  ZR_ASSERT_EQ_U32((uint32_t)parsed.xtversion_id, (uint32_t)ZR_TERM_KITTY);
  ZR_ASSERT_EQ_U32(parsed.da1_has_sixel, 0u);
  ZR_ASSERT_EQ_U32(parsed.decrqm_2026_value, 1u);
}

ZR_TEST_GOLDEN(detect_fixture_xterm_sixel_response_set) {
  static const uint8_t kBytes[] = "\x1b[?1;2;4;22c"
                                  "\x1b[>41;3600;0c"
                                  "\x1b[6;16;8t";

  ZR_ASSERT_EQ_U32((uint32_t)zr_golden_compare_fixture("detect_xterm_sixel", kBytes, sizeof(kBytes) - 1u), 0u);
  const zr_detect_parsed_t parsed = zr_golden_parse_bytes(kBytes, sizeof(kBytes) - 1u);
  ZR_ASSERT_EQ_U32(parsed.da1_responded, 1u);
  ZR_ASSERT_EQ_U32(parsed.da1_has_sixel, 1u);
  ZR_ASSERT_EQ_U32(parsed.da2_responded, 1u);
}

ZR_TEST_GOLDEN(detect_fixture_minimal_da1_only) {
  static const uint8_t kBytes[] = "\x1b[?1;2;22c";

  ZR_ASSERT_EQ_U32((uint32_t)zr_golden_compare_fixture("detect_minimal_da1", kBytes, sizeof(kBytes) - 1u), 0u);
  const zr_detect_parsed_t parsed = zr_golden_parse_bytes(kBytes, sizeof(kBytes) - 1u);
  ZR_ASSERT_EQ_U32(parsed.da1_responded, 1u);
  ZR_ASSERT_EQ_U32(parsed.da2_responded, 0u);
  ZR_ASSERT_EQ_U32(parsed.xtversion_responded, 0u);
}

ZR_TEST_GOLDEN(detect_fixture_empty_timeout) {
  static const uint8_t kBytes[] = "";

  ZR_ASSERT_EQ_U32((uint32_t)zr_golden_compare_fixture("detect_empty_timeout", kBytes, sizeof(kBytes) - 1u), 0u);
  const zr_detect_parsed_t parsed = zr_golden_parse_bytes(kBytes, sizeof(kBytes) - 1u);
  ZR_ASSERT_EQ_U32(parsed.xtversion_responded, 0u);
  ZR_ASSERT_EQ_U32(parsed.da1_responded, 0u);
  ZR_ASSERT_EQ_U32(parsed.da2_responded, 0u);
}
