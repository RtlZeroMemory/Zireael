/*
  tests/golden/golden_image_protocols.c — Golden fixtures for image protocols.

  Why: Pins byte-for-byte protocol sequences (Kitty/Sixel/iTerm2) so future
  changes cannot silently alter terminal output contracts.
*/

#include "zr_test.h"

#include "golden/zr_golden.h"

#include "core/zr_image.h"

#include <stdint.h>

ZR_TEST_GOLDEN(image_kitty_transmit_rgba_1x1) {
  uint8_t out[1024];
  zr_sb_t sb;
  const uint8_t rgba[4] = {1u, 2u, 3u, 255u};

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_transmit_rgba(&sb, 7u, rgba, 1u, 1u, 1u, 1u), ZR_OK);
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("image_kitty_transmit_rgba_1x1", out, sb.len) == 0);
}

ZR_TEST_GOLDEN(image_kitty_place_2_3) {
  uint8_t out[256];
  zr_sb_t sb;

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_place(&sb, 7u, 2u, 3u, 4u, 5u, -1), ZR_OK);
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("image_kitty_place_2_3", out, sb.len) == 0);
}

ZR_TEST_GOLDEN(image_kitty_delete_7) {
  uint8_t out[128];
  zr_sb_t sb;

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_image_kitty_emit_delete(&sb, 7u), ZR_OK);
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("image_kitty_delete_7", out, sb.len) == 0);
}

ZR_TEST_GOLDEN(image_sixel_rgba_1x1_red) {
  uint8_t out[1024];
  zr_sb_t sb;
  zr_arena_t arena;
  const uint8_t rgba[4] = {255u, 0u, 0u, 255u};

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_image_sixel_emit_rgba(&sb, &arena, rgba, 1u, 1u, 0u, 0u), ZR_OK);
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("image_sixel_rgba_1x1_red", out, sb.len) == 0);
  zr_arena_release(&arena);
}

ZR_TEST_GOLDEN(image_iterm2_png_small) {
  uint8_t out[1024];
  zr_sb_t sb;
  const uint8_t png_bytes[2] = {0x89u, 0x50u};

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_png(&sb, png_bytes, sizeof(png_bytes), 2u, 1u, 4u, 5u), ZR_OK);
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("image_iterm2_png_small", out, sb.len) == 0);
}

ZR_TEST_GOLDEN(image_iterm2_rgba_1x1) {
  uint8_t out[4096];
  zr_sb_t sb;
  zr_arena_t arena;
  const uint8_t rgba[4] = {1u, 2u, 3u, 255u};

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_rgba(&sb, &arena, rgba, 1u, 1u, 0u, 0u, 1u, 1u), ZR_OK);
  ZR_ASSERT_TRUE(zr_golden_compare_fixture("image_iterm2_rgba_1x1", out, sb.len) == 0);
  zr_arena_release(&arena);
}
