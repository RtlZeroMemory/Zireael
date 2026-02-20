/*
  tests/unit/test_image_kitty.c — Unit tests for Kitty protocol emitters.

  Why: These emitters are byte-level protocol encoders; exact output stability
  is required for deterministic rendering and golden fixtures.
*/

#include "zr_test.h"

#include "core/zr_image.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int zr_mem_contains(const uint8_t* haystack, size_t haystack_len, const uint8_t* needle, size_t needle_len) {
  size_t i = 0u;
  if (!haystack || !needle || needle_len == 0u || needle_len > haystack_len) {
    return 0;
  }
  for (i = 0u; i + needle_len <= haystack_len; i++) {
    if (memcmp(haystack + i, needle, needle_len) == 0) {
      return 1;
    }
  }
  return 0;
}

ZR_TEST_UNIT(image_kitty_emit_transmit_rgba_small_exact_bytes) {
  uint8_t out[512];
  zr_sb_t sb;
  const uint8_t rgba[4] = {1u, 2u, 3u, 255u};
  static const uint8_t expected[] = "\x1b_Ga=t,f=32,s=1,v=1,i=7,m=0;AQID/w==\x1b\\";

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_transmit_rgba(&sb, 7u, rgba, 1u, 1u, 1u, 1u), ZR_OK);
  ZR_ASSERT_TRUE(sb.len == (sizeof(expected) - 1u));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected) - 1u);
}

ZR_TEST_UNIT(image_kitty_emit_place_exact_bytes) {
  uint8_t out[256];
  zr_sb_t sb;
  static const uint8_t expected[] = "\x1b[4;3H\x1b_Ga=p,i=7,c=4,r=5,z=-1\x1b\\";

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_place(&sb, 7u, 2u, 3u, 4u, 5u, -1), ZR_OK);
  ZR_ASSERT_TRUE(sb.len == (sizeof(expected) - 1u));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected) - 1u);
}

ZR_TEST_UNIT(image_kitty_emit_delete_exact_bytes) {
  uint8_t out[128];
  zr_sb_t sb;
  static const uint8_t expected[] = "\x1b_Ga=d,d=i,i=7\x1b\\";

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_delete(&sb, 7u), ZR_OK);
  ZR_ASSERT_TRUE(sb.len == (sizeof(expected) - 1u));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected) - 1u);
}

ZR_TEST_UNIT(image_kitty_emit_transmit_chunks_large_payload) {
  uint8_t out[8192];
  uint8_t rgba[3076];
  zr_sb_t sb;

  static const uint8_t marker_m1[] = ",m=1;";
  static const uint8_t marker_m0[] = "\x1b\\\x1b_Gm=0;";

  for (size_t i = 0u; i < sizeof(rgba); i++) {
    rgba[i] = (uint8_t)i;
  }

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_transmit_rgba(&sb, 9u, rgba, 1u, 769u, 1u, 1u), ZR_OK);

  ZR_ASSERT_TRUE(zr_mem_contains(out, sb.len, marker_m1, sizeof(marker_m1) - 1u));
  ZR_ASSERT_TRUE(zr_mem_contains(out, sb.len, marker_m0, sizeof(marker_m0) - 1u));
}

ZR_TEST_UNIT(image_kitty_emitters_reject_invalid_arguments) {
  uint8_t out[32];
  zr_sb_t sb;
  const uint8_t rgba[4] = {0u, 0u, 0u, 255u};

  zr_sb_init(&sb, out, sizeof(out));

  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_transmit_rgba(NULL, 1u, rgba, 1u, 1u, 1u, 1u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_transmit_rgba(&sb, 0u, rgba, 1u, 1u, 1u, 1u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_transmit_rgba(&sb, 1u, NULL, 1u, 1u, 1u, 1u), ZR_ERR_INVALID_ARGUMENT);

  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_place(NULL, 1u, 0u, 0u, 1u, 1u, 0), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_place(&sb, 0u, 0u, 0u, 1u, 1u, 0), ZR_ERR_INVALID_ARGUMENT);

  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_delete(NULL, 1u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_delete(&sb, 0u), ZR_ERR_INVALID_ARGUMENT);
}
