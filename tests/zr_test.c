/*
  tests/zr_test.c — Minimal deterministic unit test harness implementation.

  Why: Provides portable test registration, deterministic ordering, and clear
  failure output for CTest. No OS/terminal dependencies.
*/

#include "zr_test.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct zr_test_ctx_t {
  const char* name;
  int failed;
  int skipped;
};

/* Registry. Keep simple and deterministic; sorted by name before running. */
#ifndef ZR_TEST_MAX
#define ZR_TEST_MAX 2048
#endif

static zr_test_case_t g_tests[ZR_TEST_MAX];
static size_t g_test_count = 0;

void zr_test_register(const char* name, zr_test_fn_t fn) {
  if (!name || !fn) {
    return;
  }
  if (g_test_count >= ZR_TEST_MAX) {
    fprintf(stderr, "zr_test: registry full (max=%d)\n", (int)ZR_TEST_MAX);
    abort();
  }
  g_tests[g_test_count].name = name;
  g_tests[g_test_count].fn = fn;
  g_test_count++;
}

static int zr_test_case_cmp_by_name(const void* a, const void* b) {
  const zr_test_case_t* ta = (const zr_test_case_t*)a;
  const zr_test_case_t* tb = (const zr_test_case_t*)b;
  const char* na = ta->name ? ta->name : "";
  const char* nb = tb->name ? tb->name : "";
  return strcmp(na, nb);
}

static int zr_has_prefix(const char* s, const char* prefix) {
  size_t i = 0;
  if (!s || !prefix) {
    return 0;
  }
  while (prefix[i] != '\0') {
    if (s[i] != prefix[i]) {
      return 0;
    }
    i++;
  }
  return 1;
}

static const char* zr_parse_prefix_arg(int argc, char** argv) {
  /* --prefix <value> */
  for (int i = 1; i + 1 < argc; i++) {
    if (argv[i] && strcmp(argv[i], "--prefix") == 0) {
      return argv[i + 1];
    }
  }
  /* --prefix=<value> */
  for (int i = 1; i < argc; i++) {
    const char* a = argv[i];
    const char* key = "--prefix=";
    if (a && zr_has_prefix(a, key)) {
      return a + strlen(key);
    }
  }
  return NULL;
}

void zr_test_fail(zr_test_ctx_t* ctx, const char* file, int line, const char* msg) {
  if (ctx) {
    ctx->failed = 1;
  }
  fprintf(stderr, "FAIL: %s:%d: %s\n", file ? file : "?", line, msg ? msg : "(null)");
}

void zr_test_failf(zr_test_ctx_t* ctx, const char* file, int line, const char* fmt, ...) {
  if (ctx) {
    ctx->failed = 1;
  }
  fprintf(stderr, "FAIL: %s:%d: ", file ? file : "?", line);
  va_list ap;
  va_start(ap, fmt);
  (void)vfprintf(stderr, fmt ? fmt : "(null)", ap);
  va_end(ap);
  fputc('\n', stderr);
}

void zr_test_skip(zr_test_ctx_t* ctx, const char* file, int line, const char* reason) {
  if (ctx) {
    ctx->skipped = 1;
  }
  fprintf(stdout, "SKIP: %s:%d: %s\n", file ? file : "?", line, reason ? reason : "(null)");
}

static void zr_hex_dump_context(FILE* out,
                                const uint8_t* expected,
                                const uint8_t* actual,
                                size_t len,
                                size_t mismatch_off) {
  /* Print up to 16 bytes before and after mismatch (32 total). */
  const size_t ctx_before = 16;
  const size_t ctx_after = 16;
  size_t start = (mismatch_off > ctx_before) ? (mismatch_off - ctx_before) : 0;
  size_t end = mismatch_off + ctx_after;
  if (end > len) {
    end = len;
  }

  fprintf(out, "  context [%zu..%zu):\n", start, end);
  fprintf(out, "    expected:");
  for (size_t i = start; i < end; i++) {
    fprintf(out, " %02X", (unsigned)expected[i]);
  }
  fputc('\n', out);
  fprintf(out, "    actual:  ");
  for (size_t i = start; i < end; i++) {
    fprintf(out, " %02X", (unsigned)actual[i]);
  }
  fputc('\n', out);
}

int zr_test_memeq(const void* actual, const void* expected, size_t len) {
  const uint8_t* a = (const uint8_t*)actual;
  const uint8_t* e = (const uint8_t*)expected;
  if (len == 0) {
    return 1;
  }
  if (!a || !e) {
    fprintf(stderr, "memeq: null pointer (len=%zu)\n", len);
    return 0;
  }
  for (size_t i = 0; i < len; i++) {
    if (a[i] != e[i]) {
      fprintf(stderr, "memeq: first mismatch at offset=%zu expected=%02X actual=%02X\n",
              i, (unsigned)e[i], (unsigned)a[i]);
      zr_hex_dump_context(stderr, e, a, len, i);
      return 0;
    }
  }
  return 1;
}

static void zr_print_usage(FILE* out) {
  fprintf(out, "usage: zireael_unit_tests [--prefix <prefix>] [--list]\n");
}

static int zr_parse_list_flag(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (argv[i] && strcmp(argv[i], "--list") == 0) {
      return 1;
    }
  }
  return 0;
}

int zr_test_run_all(int argc, char** argv) {
  const char* prefix = zr_parse_prefix_arg(argc, argv);
  if (zr_parse_list_flag(argc, argv)) {
    qsort(g_tests, g_test_count, sizeof(g_tests[0]), zr_test_case_cmp_by_name);
    for (size_t i = 0; i < g_test_count; i++) {
      if (!prefix || zr_has_prefix(g_tests[i].name, prefix)) {
        fprintf(stdout, "%s\n", g_tests[i].name);
      }
    }
    return 0;
  }

  if (argc > 1) {
    /* Accept only the flags we explicitly support. */
    for (int i = 1; i < argc; i++) {
      const char* a = argv[i];
      if (!a) {
        continue;
      }
      if (strcmp(a, "--list") == 0) {
        continue;
      }
      if (strcmp(a, "--prefix") == 0) {
        i++; /* consume value */
        continue;
      }
      if (zr_has_prefix(a, "--prefix=")) {
        continue;
      }
      if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
        zr_print_usage(stdout);
        return 0;
      }
      fprintf(stderr, "zr_test: unknown arg: %s\n", a);
      zr_print_usage(stderr);
      return 2;
    }
  }

  qsort(g_tests, g_test_count, sizeof(g_tests[0]), zr_test_case_cmp_by_name);

  int failed = 0;
  int ran = 0;
  int skipped = 0;

  for (size_t i = 0; i < g_test_count; i++) {
    const zr_test_case_t* t = &g_tests[i];
    if (!t->name || !t->fn) {
      continue;
    }
    if (prefix && !zr_has_prefix(t->name, prefix)) {
      continue;
    }

    zr_test_ctx_t ctx = {0};
    ctx.name = t->name;

    fprintf(stdout, "RUN: %s\n", t->name);
    t->fn(&ctx);
    ran++;

    if (ctx.skipped) {
      fprintf(stdout, "SKIPPED: %s\n", t->name);
      skipped++;
    } else if (ctx.failed) {
      fprintf(stdout, "FAILED: %s\n", t->name);
      failed++;
    } else {
      fprintf(stdout, "OK: %s\n", t->name);
    }
  }

  fprintf(stdout, "SUMMARY: ran=%d failed=%d skipped=%d\n", ran, failed, skipped);
  return failed ? 1 : 0;
}

