/*
  tests/unit/test_image_iterm2.c — Unit tests for iTerm2 image emitters.

  Why: OSC 1337 output and PNG/base64 wrapping are strict byte contracts for
  wrappers and golden tests.
*/

#include "zr_test.h"

#include "core/zr_image.h"

#include <stdint.h>
#include <string.h>

static int zr_mem_count(const uint8_t* haystack, size_t haystack_len, const uint8_t* needle, size_t needle_len) {
  size_t i = 0u;
  int count = 0;
  if (!haystack || !needle || needle_len == 0u || needle_len > haystack_len) {
    return 0;
  }
  for (i = 0u; i + needle_len <= haystack_len; i++) {
    if (memcmp(haystack + i, needle, needle_len) == 0) {
      count++;
    }
  }
  return count;
}

static int zr_base64_char_is_data(uint8_t c) {
  if (c >= (uint8_t)'A' && c <= (uint8_t)'Z') {
    return 1;
  }
  if (c >= (uint8_t)'a' && c <= (uint8_t)'z') {
    return 1;
  }
  if (c >= (uint8_t)'0' && c <= (uint8_t)'9') {
    return 1;
  }
  return (c == (uint8_t)'+') || (c == (uint8_t)'/');
}

static int zr_base64_has_valid_framing(const uint8_t* b64, size_t b64_len) {
  size_t i = 0u;
  size_t pad_count = 0u;
  uint8_t seen_pad = 0u;
  if (!b64 || b64_len == 0u || (b64_len % 4u) != 0u) {
    return 0;
  }
  for (i = 0u; i < b64_len; i++) {
    const uint8_t c = b64[i];
    if (c == (uint8_t)'=') {
      seen_pad = 1u;
      pad_count++;
      continue;
    }
    if (!zr_base64_char_is_data(c) || seen_pad != 0u) {
      return 0;
    }
  }
  if (pad_count > 2u) {
    return 0;
  }
  if (pad_count == 1u && b64[b64_len - 1u] != (uint8_t)'=') {
    return 0;
  }
  if (pad_count == 2u && (b64[b64_len - 2u] != (uint8_t)'=' || b64[b64_len - 1u] != (uint8_t)'=')) {
    return 0;
  }
  return 1;
}

static int zr_iterm2_extract_payload(const uint8_t* bytes, size_t len, const uint8_t** out_payload, size_t* out_len) {
  size_t colon_at = 0u;
  size_t i = 0u;
  if (!bytes || !out_payload || !out_len || len < 3u || bytes[len - 1u] != 0x07u) {
    return 0;
  }
  for (i = 0u; i < len; i++) {
    if (bytes[i] == (uint8_t)':') {
      colon_at = i;
      if (colon_at + 1u >= (len - 1u)) {
        return 0;
      }
      *out_payload = bytes + colon_at + 1u;
      *out_len = (len - 1u) - (colon_at + 1u);
      return 1;
    }
  }
  return 0;
}

ZR_TEST_UNIT(image_iterm2_emit_png_exact_bytes) {
  uint8_t out[512];
  zr_sb_t sb;
  const uint8_t png_bytes[2] = {0x89u, 0x50u};
  static const uint8_t expected[] =
      "\x1b[2;3H\x1b]1337;File=inline=1;width=4;height=5;preserveAspectRatio=1;size=2:iVA=\x07";
  const uint8_t* payload = NULL;
  size_t payload_len = 0u;

  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_png(&sb, png_bytes, sizeof(png_bytes), 2u, 1u, 4u, 5u), ZR_OK);
  ZR_ASSERT_TRUE(sb.len == (sizeof(expected) - 1u));
  ZR_ASSERT_MEMEQ(out, expected, sizeof(expected) - 1u);
  ZR_ASSERT_TRUE(zr_iterm2_extract_payload(out, sb.len, &payload, &payload_len) != 0);
  ZR_ASSERT_TRUE(zr_base64_has_valid_framing(payload, payload_len) != 0);
}

ZR_TEST_UNIT(image_iterm2_emit_rgba_is_deterministic) {
  uint8_t out_a[4096];
  uint8_t out_b[4096];
  zr_sb_t sb_a;
  zr_sb_t sb_b;
  zr_arena_t arena;
  const uint8_t rgba[4] = {1u, 2u, 3u, 255u};
  static const uint8_t prefix[] = "\x1b[1;1H\x1b]1337;File=inline=1;width=1;height=1;preserveAspectRatio=1;size=73:";
  static const uint8_t marker[] = "\x1b]1337;File=inline=1;";
  const uint8_t* payload = NULL;
  size_t payload_len = 0u;

  zr_sb_init(&sb_a, out_a, sizeof(out_a));
  zr_sb_init(&sb_b, out_b, sizeof(out_b));
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_rgba(&sb_a, &arena, rgba, 1u, 1u, 0u, 0u, 1u, 1u), ZR_OK);
  zr_arena_reset(&arena);
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_rgba(&sb_b, &arena, rgba, 1u, 1u, 0u, 0u, 1u, 1u), ZR_OK);

  ZR_ASSERT_TRUE(sb_a.len == sb_b.len);
  ZR_ASSERT_MEMEQ(out_a, out_b, sb_a.len);
  ZR_ASSERT_TRUE(sb_a.len > sizeof(prefix));
  ZR_ASSERT_MEMEQ(out_a, prefix, sizeof(prefix) - 1u);
  ZR_ASSERT_EQ_U32(out_a[sb_a.len - 1u], 0x07u);
  ZR_ASSERT_TRUE(zr_mem_count(out_a, sb_a.len, marker, sizeof(marker) - 1u) == 1);
  ZR_ASSERT_TRUE(zr_iterm2_extract_payload(out_a, sb_a.len, &payload, &payload_len) != 0);
  ZR_ASSERT_TRUE(zr_base64_has_valid_framing(payload, payload_len) != 0);

  zr_arena_release(&arena);
}

ZR_TEST_UNIT(image_iterm2_emitters_reject_invalid_arguments) {
  uint8_t out[64];
  uint8_t small_out[3];
  zr_sb_t sb;
  zr_sb_t small_sb;
  zr_arena_t arena;
  const uint8_t png_bytes[2] = {0x89u, 0x50u};
  const uint8_t rgba[4] = {1u, 2u, 3u, 255u};

  zr_sb_init(&sb, out, sizeof(out));
  zr_sb_init(&small_sb, small_out, sizeof(small_out));
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);

  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_png(NULL, png_bytes, sizeof(png_bytes), 0u, 0u, 1u, 1u),
                   ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_png(&sb, NULL, sizeof(png_bytes), 0u, 0u, 1u, 1u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_png(&sb, png_bytes, 0u, 0u, 0u, 1u, 1u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_png(&sb, png_bytes, sizeof(png_bytes), 0u, 0u, 0u, 1u),
                   ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_png(&small_sb, png_bytes, sizeof(png_bytes), 0u, 0u, 1u, 1u), ZR_ERR_LIMIT);

  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_rgba(NULL, &arena, rgba, 1u, 1u, 0u, 0u, 1u, 1u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_rgba(&sb, NULL, rgba, 1u, 1u, 0u, 0u, 1u, 1u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_rgba(&sb, &arena, NULL, 1u, 1u, 0u, 0u, 1u, 1u), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_iterm2_emit_rgba(&sb, &arena, rgba, 0u, 1u, 0u, 0u, 1u, 1u), ZR_ERR_INVALID_ARGUMENT);

  zr_arena_release(&arena);
}
