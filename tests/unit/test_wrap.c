/*
  tests/unit/test_wrap.c — Greedy wrapping + measurement vectors.

  Why: Pins deterministic wrapping decisions (whitespace preference, hard
  breaks, tab expansion) at grapheme boundaries.
*/

#include "zr_test.h"

#include "unicode/zr_wrap.h"

ZR_TEST_UNIT(measure_simple_and_tabs) {
  const uint8_t s[] = {'a', '\t', 'b'};
  zr_measure_utf8_t m;
  ZR_ASSERT_EQ_U32(zr_measure_utf8(s, sizeof(s), ZR_WIDTH_EMOJI_WIDE, 4u, &m), ZR_OK);
  ZR_ASSERT_EQ_U32(m.lines, 1u);
  ZR_ASSERT_EQ_U32(m.max_cols, 5u); /* 'a' (1) + tab to col 4 (+3) + 'b' (1) */
}

ZR_TEST_UNIT(wrap_prefers_whitespace_when_full) {
  const uint8_t s[] = "hello world";
  size_t        offs[8];
  size_t        n = 0u;
  bool          trunc = false;

  ZR_ASSERT_EQ_U32(zr_wrap_greedy_utf8((const uint8_t*)s, 11u, 5u, ZR_WIDTH_EMOJI_WIDE, 8u, offs, 8u, &n, &trunc),
                   ZR_OK);
  ZR_ASSERT_TRUE(!trunc);
  ZR_ASSERT_EQ_U32(n, 2u);
  ZR_ASSERT_EQ_U32(offs[0], 0u);
  ZR_ASSERT_EQ_U32(offs[1], 6u); /* skip the overflowing space */
}

ZR_TEST_UNIT(wrap_hard_break_newline) {
  const uint8_t s[] = {'a', 'b', '\n', 'c', 'd'};
  size_t        offs[8];
  size_t        n = 0u;
  bool          trunc = false;

  ZR_ASSERT_EQ_U32(zr_wrap_greedy_utf8(s, sizeof(s), 80u, ZR_WIDTH_EMOJI_WIDE, 8u, offs, 8u, &n, &trunc), ZR_OK);
  ZR_ASSERT_TRUE(!trunc);
  ZR_ASSERT_EQ_U32(n, 2u);
  ZR_ASSERT_EQ_U32(offs[0], 0u);
  ZR_ASSERT_EQ_U32(offs[1], 3u);
}

ZR_TEST_UNIT(wrap_tab_break_opportunity) {
  const uint8_t s[] = {'a', '\t', 'b'};
  size_t        offs[8];
  size_t        n = 0u;
  bool          trunc = false;

  ZR_ASSERT_EQ_U32(zr_wrap_greedy_utf8(s, sizeof(s), 4u, ZR_WIDTH_EMOJI_WIDE, 4u, offs, 8u, &n, &trunc), ZR_OK);
  ZR_ASSERT_TRUE(!trunc);
  ZR_ASSERT_EQ_U32(n, 2u);
  ZR_ASSERT_EQ_U32(offs[0], 0u);
  ZR_ASSERT_EQ_U32(offs[1], 2u);
}

ZR_TEST_UNIT(wrap_truncates_offsets_buffer) {
  const uint8_t s[] = "hello world";
  size_t        offs[1];
  size_t        n = 0u;
  bool          trunc = false;

  ZR_ASSERT_EQ_U32(zr_wrap_greedy_utf8((const uint8_t*)s, 11u, 5u, ZR_WIDTH_EMOJI_WIDE, 8u, offs, 1u, &n, &trunc),
                   ZR_OK);
  ZR_ASSERT_TRUE(trunc);
  ZR_ASSERT_EQ_U32(n, 2u);
  ZR_ASSERT_EQ_U32(offs[0], 0u);
}

