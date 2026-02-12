/*
  tests/unit/test_width.c — Width policy pins and scalar/grapheme widths.

  Why: Pins the default policy and validates deterministic width behavior for
  combining marks, CJK wide chars, and emoji policy differences.
*/

#include "zr_test.h"

#include "unicode/zr_unicode_pins.h"
#include "unicode/zr_width.h"

ZR_TEST_UNIT(unicode_version_and_default_policy_are_pinned) {
  const zr_unicode_version_t v = zr_unicode_version();
  ZR_ASSERT_EQ_U32(v.major, 15u);
  ZR_ASSERT_EQ_U32(v.minor, 1u);
  ZR_ASSERT_EQ_U32(v.patch, 0u);
  ZR_ASSERT_EQ_U32(zr_width_policy_default(), (uint32_t)ZR_WIDTH_EMOJI_WIDE);
}

ZR_TEST_UNIT(width_codepoint_vectors) {
  ZR_ASSERT_EQ_U32(zr_width_codepoint(0x0041u), 1u); /* 'A' */
  ZR_ASSERT_EQ_U32(zr_width_codepoint(0x0301u), 0u); /* combining acute */
  ZR_ASSERT_EQ_U32(zr_width_codepoint(0x4E00u), 2u); /* CJK ideograph */
}

ZR_TEST_UNIT(width_grapheme_emoji_policy_vectors) {
  /* U+1F600 ("😀") policy width. */
  const uint8_t s[] = {0xF0u, 0x9Fu, 0x98u, 0x80u};
  ZR_ASSERT_EQ_U32(zr_width_grapheme_utf8(s, sizeof(s), ZR_WIDTH_EMOJI_WIDE), 2u);
  ZR_ASSERT_EQ_U32(zr_width_grapheme_utf8(s, sizeof(s), ZR_WIDTH_EMOJI_NARROW), 1u);
}

ZR_TEST_UNIT(width_grapheme_zwj_sequence_uses_emoji_policy) {
  /* U+1F600 ZWJ U+1F600 ("😀‍😀"). */
  const uint8_t s[] = {0xF0u, 0x9Fu, 0x98u, 0x80u, 0xE2u, 0x80u, 0x8Du, 0xF0u, 0x9Fu, 0x98u, 0x80u};
  ZR_ASSERT_EQ_U32(zr_width_grapheme_utf8(s, sizeof(s), ZR_WIDTH_EMOJI_WIDE), 2u);
  ZR_ASSERT_EQ_U32(zr_width_grapheme_utf8(s, sizeof(s), ZR_WIDTH_EMOJI_NARROW), 1u);
}

ZR_TEST_UNIT(width_grapheme_vs16_sequence_uses_emoji_policy) {
  /* U+2764 U+FE0F ("❤️"). */
  const uint8_t s[] = {0xE2u, 0x9Du, 0xA4u, 0xEFu, 0xB8u, 0x8Fu};
  ZR_ASSERT_EQ_U32(zr_width_grapheme_utf8(s, sizeof(s), ZR_WIDTH_EMOJI_WIDE), 2u);
  ZR_ASSERT_EQ_U32(zr_width_grapheme_utf8(s, sizeof(s), ZR_WIDTH_EMOJI_NARROW), 1u);
}

ZR_TEST_UNIT(width_grapheme_keycap_sequence_uses_emoji_policy) {
  /* U+0031 U+FE0F U+20E3 ("1️⃣"). */
  const uint8_t keycap[] = {0x31u, 0xEFu, 0xB8u, 0x8Fu, 0xE2u, 0x83u, 0xA3u};
  ZR_ASSERT_EQ_U32(zr_width_grapheme_utf8(keycap, sizeof(keycap), ZR_WIDTH_EMOJI_WIDE), 2u);
  ZR_ASSERT_EQ_U32(zr_width_grapheme_utf8(keycap, sizeof(keycap), ZR_WIDTH_EMOJI_NARROW), 1u);
}

ZR_TEST_UNIT(width_grapheme_combining_sequence) {
  /* "e" + U+0301. */
  const uint8_t s[] = {0x65u, 0xCCu, 0x81u};
  ZR_ASSERT_EQ_U32(zr_width_grapheme_utf8(s, sizeof(s), ZR_WIDTH_EMOJI_WIDE), 1u);
}
