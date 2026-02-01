/*
  tests/unit/test_grapheme_iter.c — Grapheme iterator vectors (UAX #29 subset).

  Why: Pins stable grapheme boundaries for combining marks, RI flags, and a
  basic ZWJ emoji sequence without relying on libc/OS facilities.
*/

#include "zr_test.h"

#include "unicode/zr_grapheme.h"

static void zr_assert_one_cluster(zr_test_ctx_t* ctx, const uint8_t* bytes, size_t len, size_t expect_size) {
  zr_grapheme_iter_t it;
  zr_grapheme_iter_init(&it, bytes, len);

  zr_grapheme_t g;
  ZR_ASSERT_TRUE(zr_grapheme_next(&it, &g));
  ZR_ASSERT_EQ_U32(g.offset, 0u);
  ZR_ASSERT_EQ_U32(g.size, (uint32_t)expect_size);
  ZR_ASSERT_TRUE(!zr_grapheme_next(&it, &g));
}

ZR_TEST_UNIT(grapheme_combining_mark_stays_with_base) {
  /* "e" + U+0301 (COMBINING ACUTE ACCENT). */
  const uint8_t s[] = {0x65u, 0xCCu, 0x81u};
  zr_assert_one_cluster(ctx, s, sizeof(s), sizeof(s));
}

ZR_TEST_UNIT(grapheme_regional_indicator_flag_pair) {
  /* U+1F1FA U+1F1F8 ("🇺🇸"). */
  const uint8_t s[] = {0xF0u, 0x9Fu, 0x87u, 0xBAu, 0xF0u, 0x9Fu, 0x87u, 0xB8u};
  zr_assert_one_cluster(ctx, s, sizeof(s), sizeof(s));
}

ZR_TEST_UNIT(grapheme_zwj_extended_pictographic_sequence) {
  /* U+1F469 ZWJ U+1F4BB ("👩‍💻"). */
  const uint8_t s[] = {0xF0u, 0x9Fu, 0x91u, 0xA9u, 0xE2u, 0x80u, 0x8Du, 0xF0u, 0x9Fu, 0x92u, 0xBBu};
  zr_assert_one_cluster(ctx, s, sizeof(s), sizeof(s));
}

ZR_TEST_UNIT(grapheme_iter_progress_on_malformed_utf8) {
  const uint8_t s[] = {0xF0u, 0x28u, 0x8Cu, 0x28u};

  zr_grapheme_iter_t it;
  zr_grapheme_iter_init(&it, s, sizeof(s));

  size_t seen = 0u;
  size_t total = 0u;
  zr_grapheme_t g;
  while (zr_grapheme_next(&it, &g)) {
    ZR_ASSERT_TRUE(g.size >= 1u);
    total += g.size;
    seen++;
    ZR_ASSERT_TRUE(seen <= sizeof(s));
  }
  ZR_ASSERT_EQ_U32(total, (uint32_t)sizeof(s));
}

