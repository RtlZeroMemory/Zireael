/*
  tests/golden/zr_golden.c — Golden fixture loader + mismatch diagnostics.

  Why: Provides deterministic, byte-for-byte comparisons and prints the first
  mismatch offset plus hex context to aid debugging.
*/

#include "zr_golden.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZR_TESTS_DIR
/* Fallback for ad-hoc runs; CMake should define this for CTest determinism. */
#define ZR_TESTS_DIR "tests"
#endif

static int zr_golden_build_expected_path(char* out, size_t out_cap, const char* fixture_id) {
  if (!out || out_cap == 0 || !fixture_id || fixture_id[0] == '\0') {
    return 0;
  }
  int n = snprintf(out, out_cap, "%s/golden/fixtures/%s/expected.bin", ZR_TESTS_DIR, fixture_id);
  if (n < 0) {
    return 0;
  }
  return ((size_t)n < out_cap) ? 1 : 0;
}

static uint8_t* zr_golden_read_file(const char* path, size_t* out_len) {
  if (out_len) {
    *out_len = 0;
  }
  FILE* f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return NULL;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return NULL;
  }

  size_t len = (size_t)sz;
  /*
    Allocate at least 1 byte so callers can distinguish "empty file" (success)
    from "open/read failed" (NULL).
  */
  uint8_t* buf = (uint8_t*)malloc((len > 0) ? len : 1u);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  if (len > 0) {
    size_t got = fread(buf, 1u, len, f);
    if (got != len) {
      free(buf);
      fclose(f);
      return NULL;
    }
  }

  fclose(f);
  if (out_len) {
    *out_len = len;
  }
  return buf;
}

static void zr_golden_print_hex_context(FILE* out,
                                        const uint8_t* expected,
                                        size_t expected_len,
                                        const uint8_t* actual,
                                        size_t actual_len,
                                        size_t mismatch_off) {
  const size_t ctx_before = 16;
  const size_t ctx_after = 16;

  size_t max_len = expected_len;
  if (actual_len > max_len) {
    max_len = actual_len;
  }

  size_t start = (mismatch_off > ctx_before) ? (mismatch_off - ctx_before) : 0;
  size_t end = mismatch_off + ctx_after;
  if (end > max_len) {
    end = max_len;
  }

  fprintf(out, "  context [%zu..%zu):\n", start, end);
  fprintf(out, "    expected:");
  for (size_t i = start; i < end; i++) {
    if (i < expected_len) {
      fprintf(out, " %02X", (unsigned)expected[i]);
    } else {
      fprintf(out, " --");
    }
  }
  fputc('\n', out);
  fprintf(out, "    actual:  ");
  for (size_t i = start; i < end; i++) {
    if (i < actual_len) {
      fprintf(out, " %02X", (unsigned)actual[i]);
    } else {
      fprintf(out, " --");
    }
  }
  fputc('\n', out);
}

int zr_golden_compare_fixture(const char* fixture_id, const uint8_t* actual, size_t actual_len) {
  char path[1024];
  if (!zr_golden_build_expected_path(path, sizeof(path), fixture_id)) {
    fprintf(stderr, "GOLDEN: invalid fixture id\n");
    return 3;
  }

  size_t expected_len = 0;
  uint8_t* expected = zr_golden_read_file(path, &expected_len);
  if (!expected && expected_len == 0) {
    /* Differentiate missing vs read error best-effort. */
    FILE* probe = fopen(path, "rb");
    if (!probe) {
      fprintf(stderr, "GOLDEN: missing fixture id=%s expected=%s\n",
              fixture_id ? fixture_id : "(null)", path);
      return 2;
    }
    fclose(probe);
    fprintf(stderr, "GOLDEN: failed to read fixture id=%s expected=%s\n",
            fixture_id ? fixture_id : "(null)", path);
    return 3;
  }

  /* Length mismatch is a mismatch at offset min(len). */
  size_t min_len = (expected_len < actual_len) ? expected_len : actual_len;
  size_t mismatch_off = (expected_len == actual_len) ? (size_t)-1 : min_len;

  if (expected_len == actual_len) {
    mismatch_off = (size_t)-1;
    for (size_t i = 0; i < expected_len; i++) {
      if (expected[i] != actual[i]) {
        mismatch_off = i;
        break;
      }
    }
  }

  if (mismatch_off == (size_t)-1) {
    free(expected);
    return 0;
  }

  fprintf(stderr, "GOLDEN: mismatch fixture id=%s\n", fixture_id ? fixture_id : "(null)");
  fprintf(stderr, "  expected_len=%zu actual_len=%zu\n", expected_len, actual_len);
  if (mismatch_off < expected_len && mismatch_off < actual_len) {
    fprintf(stderr, "  first_mismatch_offset=%zu expected=%02X actual=%02X\n", mismatch_off,
            (unsigned)expected[mismatch_off], (unsigned)actual[mismatch_off]);
  } else {
    fprintf(stderr, "  first_mismatch_offset=%zu (length mismatch)\n", mismatch_off);
  }
  zr_golden_print_hex_context(stderr, expected, expected_len, actual, actual_len, mismatch_off);

  free(expected);
  return 1;
}
