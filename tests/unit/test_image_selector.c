/*
  tests/unit/test_image_selector.c — Unit tests for image protocol selection.

  Why: DRAW_IMAGE auto/explicit protocol resolution must be deterministic so
  wrappers can rely on stable fallback behavior.
*/

#include "zr_test.h"

#include "core/zr_image.h"

#include <stdint.h>
#include <string.h>

ZR_TEST_UNIT(image_selector_explicit_requests_ignore_profile) {
  zr_terminal_profile_t profile;
  memset(&profile, 0, sizeof(profile));

  ZR_ASSERT_EQ_U32(zr_image_select_protocol((uint8_t)ZR_IMG_PROTO_KITTY, &profile), ZR_IMG_PROTO_KITTY);
  ZR_ASSERT_EQ_U32(zr_image_select_protocol((uint8_t)ZR_IMG_PROTO_SIXEL, &profile), ZR_IMG_PROTO_SIXEL);
  ZR_ASSERT_EQ_U32(zr_image_select_protocol((uint8_t)ZR_IMG_PROTO_ITERM2, &profile), ZR_IMG_PROTO_ITERM2);
}

ZR_TEST_UNIT(image_selector_auto_prefers_kitty_then_sixel_then_iterm2) {
  zr_terminal_profile_t profile;

  memset(&profile, 0, sizeof(profile));
  profile.supports_iterm2_images = 1u;
  ZR_ASSERT_EQ_U32(zr_image_select_protocol(0u, &profile), ZR_IMG_PROTO_ITERM2);

  memset(&profile, 0, sizeof(profile));
  profile.supports_sixel = 1u;
  profile.supports_iterm2_images = 1u;
  ZR_ASSERT_EQ_U32(zr_image_select_protocol(0u, &profile), ZR_IMG_PROTO_SIXEL);

  memset(&profile, 0, sizeof(profile));
  profile.supports_kitty_graphics = 1u;
  profile.supports_sixel = 1u;
  profile.supports_iterm2_images = 1u;
  ZR_ASSERT_EQ_U32(zr_image_select_protocol(0u, &profile), ZR_IMG_PROTO_KITTY);
}

ZR_TEST_UNIT(image_selector_rejects_invalid_request_and_missing_profile) {
  zr_terminal_profile_t profile;
  memset(&profile, 0, sizeof(profile));

  ZR_ASSERT_EQ_U32(zr_image_select_protocol(0u, NULL), ZR_IMG_PROTO_NONE);
  ZR_ASSERT_EQ_U32(zr_image_select_protocol(99u, &profile), ZR_IMG_PROTO_NONE);
}

ZR_TEST_UNIT(image_hash_fnv1a64_matches_known_vector) {
  static const uint8_t abc[] = {'a', 'b', 'c'};
  const uint64_t hash = zr_image_hash_fnv1a64(abc, sizeof(abc));

  ZR_ASSERT_TRUE(hash == 0xE71FA2190541574Bull);
  ZR_ASSERT_TRUE(hash == zr_image_hash_fnv1a64(abc, sizeof(abc)));
}

ZR_TEST_UNIT(image_hash_fnv1a64_null_guard) {
  ZR_ASSERT_TRUE(zr_image_hash_fnv1a64(NULL, 4u) == 0u);
  ZR_ASSERT_TRUE(zr_image_hash_fnv1a64(NULL, 0u) == 14695981039346656037ull);
}
