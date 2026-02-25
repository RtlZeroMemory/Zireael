/*
  tests/unit/test_wrap.c — Greedy wrapping + measurement vectors.

  Why: Pins deterministic wrapping decisions (whitespace preference, hard
  breaks, tab expansion) at grapheme boundaries.
*/

#include "zr_test.h"

#include "unicode/zr_wrap.h"

enum {
  ZR_TEST_WRAP_OFFSETS_CAP = 8u,
};

ZR_TEST_UNIT(measure_simple_and_tabs) {
  const uint8_t s[] = {'a', '\t', 'b'};
  zr_measure_utf8_t m;
  ZR_ASSERT_EQ_U32(zr_measure_utf8(s, sizeof(s), ZR_WIDTH_EMOJI_WIDE, 4u, &m), ZR_OK);
  ZR_ASSERT_EQ_U32(m.lines, 1u);
  ZR_ASSERT_EQ_U32(m.max_cols, 5u); /* 'a' (1) + tab to col 4 (+3) + 'b' (1) */
}

ZR_TEST_UNIT(measure_tab_exact_multiple_advances_full_tab_stop) {
  const uint8_t s[] = {'a', 'b', 'c', 'd', '\t', 'e'};
  zr_measure_utf8_t m;
  ZR_ASSERT_EQ_U32(zr_measure_utf8(s, sizeof(s), ZR_WIDTH_EMOJI_WIDE, 4u, &m), ZR_OK);
  ZR_ASSERT_EQ_U32(m.lines, 1u);
  ZR_ASSERT_EQ_U32(m.max_cols, 9u); /* "abcd" (4) + tab at exact stop (+4) + 'e' (1) */
}

ZR_TEST_UNIT(wrap_prefers_whitespace_when_full) {
  const uint8_t s[] = "hello world";
  size_t offs[ZR_TEST_WRAP_OFFSETS_CAP];
  size_t n = 0u;
  bool trunc = false;

  ZR_ASSERT_EQ_U32(zr_wrap_greedy_utf8((const uint8_t*)s, 11u, 5u, ZR_WIDTH_EMOJI_WIDE, 8u, offs,
                                       ZR_TEST_WRAP_OFFSETS_CAP, &n, &trunc),
                   ZR_OK);
  ZR_ASSERT_TRUE(!trunc);
  ZR_ASSERT_EQ_U32(n, 2u);
  ZR_ASSERT_EQ_U32(offs[0], 0u);
  ZR_ASSERT_EQ_U32(offs[1], 6u); /* skip the overflowing space */
}

ZR_TEST_UNIT(wrap_hard_break_newline) {
  const uint8_t s[] = {'a', 'b', '\n', 'c', 'd'};
  size_t offs[ZR_TEST_WRAP_OFFSETS_CAP];
  size_t n = 0u;
  bool trunc = false;

  ZR_ASSERT_EQ_U32(
      zr_wrap_greedy_utf8(s, sizeof(s), 80u, ZR_WIDTH_EMOJI_WIDE, 8u, offs, ZR_TEST_WRAP_OFFSETS_CAP, &n, &trunc),
      ZR_OK);
  ZR_ASSERT_TRUE(!trunc);
  ZR_ASSERT_EQ_U32(n, 2u);
  ZR_ASSERT_EQ_U32(offs[0], 0u);
  ZR_ASSERT_EQ_U32(offs[1], 3u);
}

ZR_TEST_UNIT(wrap_tab_break_opportunity) {
  const uint8_t s[] = {'a', '\t', 'b'};
  size_t offs[ZR_TEST_WRAP_OFFSETS_CAP];
  size_t n = 0u;
  bool trunc = false;

  ZR_ASSERT_EQ_U32(
      zr_wrap_greedy_utf8(s, sizeof(s), 4u, ZR_WIDTH_EMOJI_WIDE, 4u, offs, ZR_TEST_WRAP_OFFSETS_CAP, &n, &trunc),
      ZR_OK);
  ZR_ASSERT_TRUE(!trunc);
  ZR_ASSERT_EQ_U32(n, 2u);
  ZR_ASSERT_EQ_U32(offs[0], 0u);
  ZR_ASSERT_EQ_U32(offs[1], 2u);
}

ZR_TEST_UNIT(wrap_wide_grapheme_overflow_on_empty_line_forces_progress) {
  /* U+4E00 ('一') is width 2; max_cols=1 must still make forward progress. */
  const uint8_t s[] = {0xE4u, 0xB8u, 0x80u, 'a'};
  size_t offs[ZR_TEST_WRAP_OFFSETS_CAP];
  size_t n = 0u;
  bool trunc = false;

  ZR_ASSERT_EQ_U32(
      zr_wrap_greedy_utf8(s, sizeof(s), 1u, ZR_WIDTH_EMOJI_WIDE, 4u, offs, ZR_TEST_WRAP_OFFSETS_CAP, &n, &trunc),
      ZR_OK);
  ZR_ASSERT_TRUE(!trunc);
  ZR_ASSERT_EQ_U32(n, 2u);
  ZR_ASSERT_EQ_U32(offs[0], 0u);
  ZR_ASSERT_EQ_U32(offs[1], 3u); /* break before trailing 'a' */
}

ZR_TEST_UNIT(wrap_tab_exact_multiple_prefers_break_after_tab) {
  const uint8_t s[] = {'a', 'b', 'c', 'd', '\t', 'x'};
  size_t offs[ZR_TEST_WRAP_OFFSETS_CAP];
  size_t n = 0u;
  bool trunc = false;

  ZR_ASSERT_EQ_U32(
      zr_wrap_greedy_utf8(s, sizeof(s), 8u, ZR_WIDTH_EMOJI_WIDE, 4u, offs, ZR_TEST_WRAP_OFFSETS_CAP, &n, &trunc),
      ZR_OK);
  ZR_ASSERT_TRUE(!trunc);
  ZR_ASSERT_EQ_U32(n, 2u);
  ZR_ASSERT_EQ_U32(offs[0], 0u);
  ZR_ASSERT_EQ_U32(offs[1], 5u); /* break opportunity consumed after tab */
}

ZR_TEST_UNIT(wrap_truncates_offsets_buffer) {
  const uint8_t s[] = "hello world";
  size_t offs[1];
  size_t n = 0u;
  bool trunc = false;

  ZR_ASSERT_EQ_U32(zr_wrap_greedy_utf8((const uint8_t*)s, 11u, 5u, ZR_WIDTH_EMOJI_WIDE, 8u, offs, 1u, &n, &trunc),
                   ZR_OK);
  ZR_ASSERT_TRUE(trunc);
  ZR_ASSERT_EQ_U32(n, 2u);
  ZR_ASSERT_EQ_U32(offs[0], 0u);
}
