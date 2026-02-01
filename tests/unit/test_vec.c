/*
  tests/unit/test_vec.c — Unit tests for util/zr_vec.h.
*/

#include "zr_test.h"

#include "util/zr_vec.h"

#include <stdint.h>

ZR_TEST_UNIT(vec_push_limit_no_mutate) {
  uint32_t backing[3] = {0u, 0u, 0u};
  zr_vec_t v;
  ZR_ASSERT_EQ_U32(zr_vec_init(&v, backing, 3u, sizeof(uint32_t)), ZR_OK);

  const uint32_t a = 10u;
  const uint32_t b = 20u;
  const uint32_t c = 30u;
  const uint32_t d = 40u;

  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &a), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &b), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &c), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_vec_len(&v), 3u);

  /* Full push must not mutate. */
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &d), ZR_ERR_LIMIT);
  ZR_ASSERT_EQ_U32(zr_vec_len(&v), 3u);
  ZR_ASSERT_EQ_U32(*(const uint32_t*)zr_vec_at_const(&v, 2u), 30u);
}

ZR_TEST_UNIT(vec_pop) {
  uint32_t backing[2] = {0u, 0u};
  zr_vec_t v;
  ZR_ASSERT_EQ_U32(zr_vec_init(&v, backing, 2u, sizeof(uint32_t)), ZR_OK);

  const uint32_t a = 111u;
  const uint32_t b = 222u;
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &a), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &b), ZR_OK);

  uint32_t out = 0u;
  ZR_ASSERT_EQ_U32(zr_vec_pop(&v, &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out, 222u);
  ZR_ASSERT_EQ_U32(zr_vec_len(&v), 1u);

  ZR_ASSERT_EQ_U32(zr_vec_pop(&v, &out), ZR_OK);
  ZR_ASSERT_EQ_U32(out, 111u);
  ZR_ASSERT_EQ_U32(zr_vec_len(&v), 0u);

  ZR_ASSERT_TRUE(zr_vec_pop(&v, &out) != ZR_OK);
}

ZR_TEST_UNIT(vec_zero_cap_allows_null_backing) {
  zr_vec_t v;
  ZR_ASSERT_EQ_U32(zr_vec_init(&v, NULL, 0u, sizeof(uint32_t)), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_vec_len(&v), 0u);
  ZR_ASSERT_EQ_U32(zr_vec_cap(&v), 0u);

  const uint32_t x = 1u;
  ZR_ASSERT_EQ_U32(zr_vec_push(&v, &x), ZR_ERR_LIMIT);
}
