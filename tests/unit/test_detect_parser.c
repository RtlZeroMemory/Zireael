/*
  tests/unit/test_detect_parser.c — Unit coverage for startup detection parser.

  Why: Validates deterministic parsing for terminal probe responses without
  requiring a real terminal backend.
*/

#include "zr_test.h"

#include "core/zr_detect.h"

static zr_detect_parsed_t zr_test_parse_bytes(const uint8_t* bytes, size_t len) {
  zr_detect_parsed_t parsed;
  zr_detect_parsed_reset(&parsed);
  (void)zr_detect_parse_responses(bytes, len, &parsed);
  return parsed;
}

ZR_TEST_UNIT(detect_parser_xtversion_known_terminal) {
  static const uint8_t kBytes[] = "\x1bP>|kitty(0.35.0)\x1b\\";

  const zr_detect_parsed_t parsed = zr_test_parse_bytes(kBytes, sizeof(kBytes) - 1u);
  ZR_ASSERT_EQ_U32(parsed.xtversion_responded, 1u);
  ZR_ASSERT_EQ_U32((uint32_t)parsed.xtversion_id, (uint32_t)ZR_TERM_KITTY);
}

ZR_TEST_UNIT(detect_query_batch_matches_expected_bytes) {
  static const uint8_t kExpected[] = "\x1b[>0q"
                                     "\x1b[c"
                                     "\x1b[>c"
                                     "\x1b[?2026$p"
                                     "\x1b[?2027$p"
                                     "\x1b[?1016$p"
                                     "\x1b[?2004$p"
                                     "\x1b[16t"
                                     "\x1b[14t";

  size_t batch_len = 0u;
  const uint8_t* batch = zr_detect_query_batch_bytes(&batch_len);
  ZR_ASSERT_TRUE(batch != NULL);
  ZR_ASSERT_EQ_U32((uint32_t)batch_len, (uint32_t)(sizeof(kExpected) - 1u));
  ZR_ASSERT_MEMEQ(batch, kExpected, batch_len);
}

ZR_TEST_UNIT(detect_parser_xtversion_truncated_ignored) {
  static const uint8_t kBytes[] = "\x1bP>|kitty(0.35.0)";

  const zr_detect_parsed_t parsed = zr_test_parse_bytes(kBytes, sizeof(kBytes) - 1u);
  ZR_ASSERT_EQ_U32(parsed.xtversion_responded, 0u);
  ZR_ASSERT_EQ_U32((uint32_t)parsed.xtversion_id, (uint32_t)ZR_TERM_UNKNOWN);
}

ZR_TEST_UNIT(detect_parser_da1_detects_sixel) {
  static const uint8_t kBytes[] = "\x1b[?1;2;4;22c";

  const zr_detect_parsed_t parsed = zr_test_parse_bytes(kBytes, sizeof(kBytes) - 1u);
  ZR_ASSERT_EQ_U32(parsed.da1_responded, 1u);
  ZR_ASSERT_EQ_U32(parsed.da1_has_sixel, 1u);
}

ZR_TEST_UNIT(detect_parser_da2_and_decrqm_modes) {
  static const uint8_t kBytes[] = "\x1b[>65;4200;0c"
                                  "\x1b[?2026;1$y"
                                  "\x1b[?2027;1$y"
                                  "\x1b[?1016;1$y"
                                  "\x1b[?2004;2$y";

  const zr_detect_parsed_t parsed = zr_test_parse_bytes(kBytes, sizeof(kBytes) - 1u);
  ZR_ASSERT_EQ_U32(parsed.da2_responded, 1u);
  ZR_ASSERT_EQ_U32(parsed.da2_model, 65u);
  ZR_ASSERT_EQ_U32(parsed.da2_version, 4200u);
  ZR_ASSERT_EQ_U32(parsed.decrqm_2026_seen, 1u);
  ZR_ASSERT_EQ_U32(parsed.decrqm_2026_value, 1u);
  ZR_ASSERT_EQ_U32(parsed.decrqm_2027_seen, 1u);
  ZR_ASSERT_EQ_U32(parsed.decrqm_2027_value, 1u);
  ZR_ASSERT_EQ_U32(parsed.decrqm_1016_seen, 1u);
  ZR_ASSERT_EQ_U32(parsed.decrqm_1016_value, 1u);
  ZR_ASSERT_EQ_U32(parsed.decrqm_2004_seen, 1u);
  ZR_ASSERT_EQ_U32(parsed.decrqm_2004_value, 2u);
}

ZR_TEST_UNIT(detect_parser_cell_and_screen_metrics) {
  static const uint8_t kBytes[] = "\x1b[6;19;10t\x1b[4;1080;1920t";

  const zr_detect_parsed_t parsed = zr_test_parse_bytes(kBytes, sizeof(kBytes) - 1u);
  ZR_ASSERT_EQ_U32(parsed.cell_height_px, 19u);
  ZR_ASSERT_EQ_U32(parsed.cell_width_px, 10u);
  ZR_ASSERT_EQ_U32(parsed.screen_height_px, 1080u);
  ZR_ASSERT_EQ_U32(parsed.screen_width_px, 1920u);
}

ZR_TEST_UNIT(detect_parser_combined_interleaved_stream) {
  static const uint8_t kBytes[] = "noise"
                                  "\x1b[?1;2c"
                                  "x"
                                  "\x1bP>|WezTerm 20240203-110809-5046fc22\x1b\\"
                                  "y"
                                  "\x1b[>65;4200;0c"
                                  "\x1b[?2026;1$y"
                                  "\x1b[6;17;9t";

  const zr_detect_parsed_t parsed = zr_test_parse_bytes(kBytes, sizeof(kBytes) - 1u);
  ZR_ASSERT_EQ_U32(parsed.xtversion_responded, 1u);
  ZR_ASSERT_EQ_U32((uint32_t)parsed.xtversion_id, (uint32_t)ZR_TERM_WEZTERM);
  ZR_ASSERT_EQ_U32(parsed.da1_responded, 1u);
  ZR_ASSERT_EQ_U32(parsed.da2_responded, 1u);
  ZR_ASSERT_EQ_U32(parsed.decrqm_2026_seen, 1u);
  ZR_ASSERT_EQ_U32(parsed.cell_height_px, 17u);
  ZR_ASSERT_EQ_U32(parsed.cell_width_px, 9u);
}
