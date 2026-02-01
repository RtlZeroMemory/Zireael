/*
  tests/unit/test_utf8_decode.c — UTF-8 decoder vectors and invalid policy pin.

  Why: Pins the project's locked UTF-8 invalid-sequence behavior and ensures
  the decoder is deterministic and bounds-safe.
*/

#include "zr_test.h"

#include "unicode/zr_utf8.h"

typedef struct zr_utf8_vec_t {
  const uint8_t* bytes;
  size_t         len;
  uint32_t       expect_scalar;
  uint8_t        expect_size;
  uint8_t        expect_valid;
} zr_utf8_vec_t;

static void zr_assert_utf8_vec(zr_test_ctx_t* ctx, zr_utf8_vec_t v) {
  zr_utf8_decode_result_t r = zr_utf8_decode_one(v.bytes, v.len);
  ZR_ASSERT_EQ_U32(r.scalar, v.expect_scalar);
  ZR_ASSERT_EQ_U32(r.size, v.expect_size);
  ZR_ASSERT_EQ_U32(r.valid, v.expect_valid);
  if (v.len > 0u) {
    ZR_ASSERT_TRUE(r.size >= 1u);
    ZR_ASSERT_TRUE((size_t)r.size <= v.len);
  } else {
    ZR_ASSERT_EQ_U32(r.size, 0u);
  }
}

ZR_TEST_UNIT(utf8_decode_vectors) {
  /* Valid sequences. */
  const uint8_t a[] = {0x41u};
  const uint8_t cent[] = {0xC2u, 0xA2u}; /* U+00A2 */
  const uint8_t euro[] = {0xE2u, 0x82u, 0xACu}; /* U+20AC */
  const uint8_t grinning[] = {0xF0u, 0x9Fu, 0x98u, 0x80u}; /* U+1F600 */

  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){a, sizeof(a), 0x0041u, 1u, 1u});
  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){cent, sizeof(cent), 0x00A2u, 2u, 1u});
  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){euro, sizeof(euro), 0x20ACu, 3u, 1u});
  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){grinning, sizeof(grinning), 0x1F600u, 4u, 1u});

  /* Locked invalid policy: {U+FFFD, valid=0, size=1} when len>0. */
  const uint8_t cont[] = {0x80u};
  const uint8_t overlong2[] = {0xC0u, 0xAFu};
  const uint8_t short3[] = {0xE2u, 0x82u};
  const uint8_t overlong3[] = {0xE0u, 0x80u, 0x80u};
  const uint8_t surrogate[] = {0xEDu, 0xA0u, 0x80u}; /* U+D800 */
  const uint8_t too_high[] = {0xF4u, 0x90u, 0x80u, 0x80u}; /* > U+10FFFF */

  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){cont, sizeof(cont), 0xFFFDu, 1u, 0u});
  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){overlong2, sizeof(overlong2), 0xFFFDu, 1u, 0u});
  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){short3, sizeof(short3), 0xFFFDu, 1u, 0u});
  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){overlong3, sizeof(overlong3), 0xFFFDu, 1u, 0u});
  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){surrogate, sizeof(surrogate), 0xFFFDu, 1u, 0u});
  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){too_high, sizeof(too_high), 0xFFFDu, 1u, 0u});

  /* len==0: must not read and must report no progress. */
  zr_assert_utf8_vec(ctx, (zr_utf8_vec_t){NULL, 0u, 0xFFFDu, 0u, 0u});
}

