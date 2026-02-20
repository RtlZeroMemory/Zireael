/*
  tests/unit/test_base64.c — Unit tests for deterministic base64 helpers.

  Why: Image protocol emitters depend on exact RFC4648 output; these tests pin
  size math, padding behavior, and error handling.
*/

#include "zr_test.h"

#include "core/zr_base64.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct zr_b64_vector_t {
  const char* input;
  const char* expected;
} zr_b64_vector_t;

static void zr_assert_b64_encode(zr_test_ctx_t* ctx, const char* input, const char* expected) {
  uint8_t out[128];
  size_t out_len = 0u;
  const size_t in_len = input ? strlen(input) : 0u;
  const size_t expected_len = expected ? strlen(expected) : 0u;

  memset(out, 0xA5, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_base64_encode((const uint8_t*)input, in_len, out, sizeof(out), &out_len), ZR_OK);
  ZR_ASSERT_TRUE(out_len == expected_len);
  ZR_ASSERT_MEMEQ(out, expected, expected_len);
}

ZR_TEST_UNIT(base64_encoded_size_common_cases) {
  uint8_t overflow = 0u;

  ZR_ASSERT_TRUE(zr_base64_encoded_size(0u, &overflow) == 0u);
  ZR_ASSERT_EQ_U32(overflow, 0u);

  ZR_ASSERT_TRUE(zr_base64_encoded_size(1u, &overflow) == 4u);
  ZR_ASSERT_EQ_U32(overflow, 0u);
  ZR_ASSERT_TRUE(zr_base64_encoded_size(2u, &overflow) == 4u);
  ZR_ASSERT_EQ_U32(overflow, 0u);
  ZR_ASSERT_TRUE(zr_base64_encoded_size(3u, &overflow) == 4u);
  ZR_ASSERT_EQ_U32(overflow, 0u);
  ZR_ASSERT_TRUE(zr_base64_encoded_size(4u, &overflow) == 8u);
  ZR_ASSERT_EQ_U32(overflow, 0u);
}

ZR_TEST_UNIT(base64_encoded_size_overflow_sets_flag) {
  uint8_t overflow = 0u;
  const size_t out = zr_base64_encoded_size(SIZE_MAX, &overflow);

  ZR_ASSERT_TRUE(out == 0u);
  ZR_ASSERT_EQ_U32(overflow, 1u);
}

ZR_TEST_UNIT(base64_encode_matches_rfc4648_vectors) {
  static const zr_b64_vector_t vectors[] = {
      {"", ""},
      {"f", "Zg=="},
      {"fo", "Zm8="},
      {"foo", "Zm9v"},
      {"foob", "Zm9vYg=="},
      {"fooba", "Zm9vYmE="},
      {"foobar", "Zm9vYmFy"},
  };

  for (size_t i = 0u; i < (sizeof(vectors) / sizeof(vectors[0])); i++) {
    zr_assert_b64_encode(ctx, vectors[i].input, vectors[i].expected);
  }
}

ZR_TEST_UNIT(base64_encode_respects_output_capacity) {
  const uint8_t input[] = {(uint8_t)'f', (uint8_t)'o', (uint8_t)'o'};
  uint8_t out[3] = {0u, 0u, 0u};
  size_t out_len = 777u;

  ZR_ASSERT_EQ_U32(zr_base64_encode(input, sizeof(input), out, sizeof(out), &out_len), ZR_ERR_LIMIT);
  ZR_ASSERT_TRUE(out_len == 0u);
}

ZR_TEST_UNIT(base64_encode_rejects_invalid_arguments) {
  const uint8_t input[] = {1u, 2u, 3u};
  uint8_t out[8] = {0u};
  size_t out_len = 0u;

  ZR_ASSERT_EQ_U32(zr_base64_encode(NULL, sizeof(input), out, sizeof(out), &out_len), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_base64_encode(input, sizeof(input), NULL, sizeof(out), &out_len), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_base64_encode(input, sizeof(input), out, sizeof(out), NULL), ZR_ERR_INVALID_ARGUMENT);

  ZR_ASSERT_EQ_U32(zr_base64_encode(NULL, 0u, out, sizeof(out), &out_len), ZR_OK);
  ZR_ASSERT_TRUE(out_len == 0u);
}
