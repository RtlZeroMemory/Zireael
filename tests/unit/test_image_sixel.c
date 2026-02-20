/*
  tests/unit/test_image_sixel.c — Unit tests for Sixel protocol emitter.

  Why: Sixel output is byte-level and capability-sensitive; this locks a small
  canonical sequence and argument validation behavior.
*/

#include "zr_test.h"

#include "core/zr_image.h"

#include <stdint.h>

ZR_TEST_UNIT(image_sixel_emit_rgba_small_exact_bytes) {
  uint8_t out[512];
  zr_sb_t sb;
  zr_arena_t arena;
  const uint8_t rgba[4] = {255u, 0u, 0u, 255u};
  static const uint8_t expected[] = "\x1b[1;1H\x1bP0;1;0q\"1;1;1;1#0;2;100;0;0#0@$-\x1b\\";

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_image_sixel_emit_rgba(&sb, &arena, rgba, 1u, 1u, 0u, 0u), ZR_OK);
  ZR_ASSERT_TRUE(sb.len == (sizeof(expected) - 1u));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected) - 1u);
  zr_arena_release(&arena);
}

ZR_TEST_UNIT(image_sixel_emit_rgba_rejects_invalid_arguments) {
  uint8_t out[64];
  zr_sb_t sb;
  zr_arena_t arena;
  const uint8_t rgba[4] = {1u, 2u, 3u, 4u};

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_image_sixel_emit_rgba(NULL, &arena, rgba, 1u, 1u, 0u, 0u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_sixel_emit_rgba(&sb, NULL, rgba, 1u, 1u, 0u, 0u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_sixel_emit_rgba(&sb, &arena, NULL, 1u, 1u, 0u, 0u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_sixel_emit_rgba(&sb, &arena, rgba, 0u, 1u, 0u, 0u), ZR_ERR_INVALID_ARGUMENT);

  zr_arena_release(&arena);
}
