/*
  tests/unit/test_sixel_quantize.c — Unit tests for Sixel quantization behavior.

  Why: Quantization and alpha handling drive protocol determinism; these tests
  lock palette ordering, transparency behavior, and RLE emission boundaries.
*/

#include "zr_test.h"

#include "core/zr_image.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int zr_mem_index_of(const uint8_t* haystack, size_t haystack_len, const uint8_t* needle, size_t needle_len) {
  size_t i = 0u;
  if (!haystack || !needle || needle_len == 0u || needle_len > haystack_len) {
    return -1;
  }
  for (i = 0u; i + needle_len <= haystack_len; i++) {
    if (memcmp(haystack + i, needle, needle_len) == 0) {
      return (int)i;
    }
  }
  return -1;
}

ZR_TEST_UNIT(sixel_quantize_alpha_below_threshold_emits_no_palette) {
  uint8_t out[256];
  zr_sb_t sb;
  zr_arena_t arena;
  const uint8_t rgba[4] = {255u, 0u, 0u, 127u};
  static const uint8_t expected[] = "\x1b[1;1H\x1bP0;1;0q\"1;1;1;1-\x1b\\";

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_image_sixel_emit_rgba(&sb, &arena, rgba, 1u, 1u, 0u, 0u), ZR_OK);
  ZR_ASSERT_TRUE(sb.len == (sizeof(expected) - 1u));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected) - 1u);

  zr_arena_release(&arena);
}

ZR_TEST_UNIT(sixel_quantize_repeated_band_uses_rle_marker) {
  uint8_t out[512];
  zr_sb_t sb;
  zr_arena_t arena;
  uint8_t rgba[16];
  static const uint8_t marker[] = "!4@";

  for (size_t i = 0u; i < 4u; i++) {
    rgba[i * 4u + 0u] = 255u;
    rgba[i * 4u + 1u] = 0u;
    rgba[i * 4u + 2u] = 0u;
    rgba[i * 4u + 3u] = 255u;
  }

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_image_sixel_emit_rgba(&sb, &arena, rgba, 4u, 1u, 0u, 0u), ZR_OK);
  ZR_ASSERT_TRUE(zr_mem_index_of(out, sb.len, marker, sizeof(marker) - 1u) >= 0);

  zr_arena_release(&arena);
}

ZR_TEST_UNIT(sixel_quantize_palette_order_is_first_seen) {
  uint8_t out[512];
  zr_sb_t sb;
  zr_arena_t arena;
  uint8_t rgba[8];
  static const uint8_t red_palette[] = "#0;2;100;0;0";
  static const uint8_t blue_palette[] = "#1;2;0;0;100";
  int red_off = -1;
  int blue_off = -1;

  rgba[0u] = 255u;
  rgba[1u] = 0u;
  rgba[2u] = 0u;
  rgba[3u] = 255u;
  rgba[4u] = 0u;
  rgba[5u] = 0u;
  rgba[6u] = 255u;
  rgba[7u] = 255u;

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_image_sixel_emit_rgba(&sb, &arena, rgba, 2u, 1u, 0u, 0u), ZR_OK);

  red_off = zr_mem_index_of(out, sb.len, red_palette, sizeof(red_palette) - 1u);
  blue_off = zr_mem_index_of(out, sb.len, blue_palette, sizeof(blue_palette) - 1u);

  ZR_ASSERT_TRUE(red_off >= 0);
  ZR_ASSERT_TRUE(blue_off >= 0);
  ZR_ASSERT_TRUE(red_off < blue_off);

  zr_arena_release(&arena);
}
