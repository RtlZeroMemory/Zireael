/*
  tests/perf/diff_hotpath_bench.c — Deterministic microbenchmark for diff hot paths.

  Why: Provides before/after evidence for renderer optimizations by measuring
  diff CPU cost, synthetic write cost, emitted bytes, and p95/p99 tail latency.
*/

#include "core/zr_diff.h"
#include "core/zr_framebuffer.h"
#include "util/zr_caps.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
  ZR_BENCH_COLS = 160u,
  ZR_BENCH_ROWS = 48u,
  ZR_BENCH_WARMUP_ITERS = 128u,
  ZR_BENCH_SAMPLE_ITERS = 1024u,
  ZR_BENCH_OUT_CAP = 8u * 1024u * 1024u,
};

typedef struct zr_bench_metrics_t {
  uint64_t diff_mean_ns;
  uint64_t diff_p95_ns;
  uint64_t diff_p99_ns;
  uint64_t write_mean_ns;
  uint64_t write_p95_ns;
  uint64_t write_p99_ns;
  uint64_t bytes_mean;
} zr_bench_metrics_t;

typedef struct zr_bench_case_t {
  const char* name;
  uint8_t enable_scroll;
} zr_bench_case_t;

static volatile uint64_t g_sink = 0u;

static uint64_t zr_now_ns(void) {
  struct timespec ts;
#if defined(CLOCK_MONOTONIC)
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0u;
  }
#else
  if (timespec_get(&ts, TIME_UTC) != TIME_UTC) {
    return 0u;
  }
#endif
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int zr_u64_cmp(const void* a, const void* b) {
  const uint64_t av = *(const uint64_t*)a;
  const uint64_t bv = *(const uint64_t*)b;
  if (av < bv) {
    return -1;
  }
  if (av > bv) {
    return 1;
  }
  return 0;
}

static uint64_t zr_percentile_ns(const uint64_t* src, size_t n, uint32_t pct_num, uint32_t pct_den) {
  if (!src || n == 0u || pct_den == 0u || pct_num > pct_den) {
    return 0u;
  }

  uint64_t* sorted = (uint64_t*)malloc(sizeof(uint64_t) * n);
  if (!sorted) {
    return 0u;
  }
  memcpy(sorted, src, sizeof(uint64_t) * n);
  qsort(sorted, n, sizeof(uint64_t), zr_u64_cmp);

  const uint64_t rank = ((uint64_t)pct_num * (uint64_t)n + (uint64_t)pct_den - 1u) / (uint64_t)pct_den;
  size_t idx = (rank == 0u) ? 0u : (size_t)(rank - 1u);
  if (idx >= n) {
    idx = n - 1u;
  }

  const uint64_t out = sorted[idx];
  free(sorted);
  return out;
}

static uint64_t zr_mean_u64(const uint64_t* values, size_t n) {
  if (!values || n == 0u) {
    return 0u;
  }

  const uint64_t n_u64 = (uint64_t)n;
  uint64_t quot_sum = 0u;
  uint64_t rem_sum = 0u;
  for (size_t i = 0u; i < n; i++) {
    quot_sum += values[i] / n_u64;
    rem_sum += values[i] % n_u64;
  }
  quot_sum += rem_sum / n_u64;
  return quot_sum;
}

static void zr_fill_clear(zr_fb_t* fb, zr_style_t style) {
  if (!fb) {
    return;
  }
  (void)zr_fb_clear(fb, &style);
}

static void zr_set_cell_ascii(zr_fb_t* fb, uint32_t x, uint32_t y, uint8_t ch, zr_style_t style) {
  zr_cell_t* c = zr_fb_cell(fb, x, y);
  if (!c) {
    return;
  }
  memset(c->glyph, 0, sizeof(c->glyph));
  c->glyph[0] = ch;
  c->glyph_len = 1u;
  c->width = 1u;
  c->style = style;
}

static void zr_case_sparse(zr_fb_t* a, zr_fb_t* b) {
  zr_style_t s = {0u, 0u, 0u, 0u};
  zr_fill_clear(a, s);
  zr_fill_clear(b, s);

  for (uint32_t y = 1u; y + 1u < ZR_BENCH_ROWS; y += 6u) {
    const uint32_t x = (y * 11u) % ZR_BENCH_COLS;
    zr_set_cell_ascii(b, x, y, (uint8_t)('A' + (y % 26u)), s);
  }
}

static void zr_case_dense(zr_fb_t* a, zr_fb_t* b) {
  zr_style_t s = {0u, 0u, 0u, 0u};
  for (uint32_t y = 0u; y < ZR_BENCH_ROWS; y++) {
    for (uint32_t x = 0u; x < ZR_BENCH_COLS; x++) {
      zr_set_cell_ascii(a, x, y, (uint8_t)('a' + ((x + y) % 26u)), s);
      zr_set_cell_ascii(b, x, y, (uint8_t)('a' + ((x + y + 13u) % 26u)), s);
    }
  }
}

static void zr_case_scroll_like(zr_fb_t* a, zr_fb_t* b) {
  zr_style_t s = {0u, 0u, 0u, 0u};
  for (uint32_t y = 0u; y < ZR_BENCH_ROWS; y++) {
    const uint8_t ch = (uint8_t)('A' + (y % 26u));
    for (uint32_t x = 0u; x < ZR_BENCH_COLS; x++) {
      zr_set_cell_ascii(a, x, y, ch, s);
    }
  }

  for (uint32_t y = 0u; y + 1u < ZR_BENCH_ROWS; y++) {
    const uint8_t ch = (uint8_t)('A' + ((y + 1u) % 26u));
    for (uint32_t x = 0u; x < ZR_BENCH_COLS; x++) {
      zr_set_cell_ascii(b, x, y, ch, s);
    }
  }
  for (uint32_t x = 0u; x < ZR_BENCH_COLS; x++) {
    zr_set_cell_ascii(b, x, ZR_BENCH_ROWS - 1u, (uint8_t)'#', s);
  }
}

static void zr_case_style_churn(zr_fb_t* a, zr_fb_t* b) {
  for (uint32_t y = 0u; y < ZR_BENCH_ROWS; y++) {
    for (uint32_t x = 0u; x < ZR_BENCH_COLS; x++) {
      zr_style_t s0 = {0x00112233u, 0x00000000u, 0u, 0u};
      zr_style_t s1 = {0x00D07010u, 0x00010101u, 0u, 0u};

      if (((x + y) & 1u) != 0u) {
        s0.attrs = 1u;
        s1.attrs = 4u;
      } else {
        s0.attrs = 8u;
        s1.attrs = 16u;
      }

      zr_set_cell_ascii(a, x, y, (uint8_t)'X', s0);
      zr_set_cell_ascii(b, x, y, (uint8_t)'X', s1);
    }
  }
}

static void zr_prepare_case(const zr_bench_case_t* bench_case, zr_fb_t* a, zr_fb_t* b) {
  if (!bench_case || !a || !b || !bench_case->name) {
    return;
  }
  if (strcmp(bench_case->name, "sparse_edits") == 0) {
    zr_case_sparse(a, b);
    return;
  }
  if (strcmp(bench_case->name, "dense_edits") == 0) {
    zr_case_dense(a, b);
    return;
  }
  if (strcmp(bench_case->name, "scroll_like") == 0) {
    zr_case_scroll_like(a, b);
    return;
  }
  if (strcmp(bench_case->name, "style_churn") == 0) {
    zr_case_style_churn(a, b);
    return;
  }
}

static int zr_run_case(const zr_bench_case_t* bench_case, zr_bench_metrics_t* out_metrics) {
  if (!bench_case || !out_metrics) {
    return 1;
  }

  zr_fb_t fb_a = {0u, 0u, NULL};
  zr_fb_t fb_b = {0u, 0u, NULL};
  if (zr_fb_init(&fb_a, ZR_BENCH_COLS, ZR_BENCH_ROWS) != ZR_OK ||
      zr_fb_init(&fb_b, ZR_BENCH_COLS, ZR_BENCH_ROWS) != ZR_OK) {
    zr_fb_release(&fb_a);
    zr_fb_release(&fb_b);
    return 1;
  }
  zr_prepare_case(bench_case, &fb_a, &fb_b);

  zr_limits_t lim = zr_limits_default();
  zr_damage_rect_t* damage = (zr_damage_rect_t*)calloc((size_t)lim.diff_max_damage_rects, sizeof(zr_damage_rect_t));
  uint64_t* prev_row_hashes = (uint64_t*)calloc((size_t)ZR_BENCH_ROWS, sizeof(uint64_t));
  uint64_t* next_row_hashes = (uint64_t*)calloc((size_t)ZR_BENCH_ROWS, sizeof(uint64_t));
  uint8_t* dirty_rows = (uint8_t*)calloc((size_t)ZR_BENCH_ROWS, sizeof(uint8_t));
  uint8_t* out_buf = (uint8_t*)malloc((size_t)ZR_BENCH_OUT_CAP);
  uint8_t* write_buf = (uint8_t*)malloc((size_t)ZR_BENCH_OUT_CAP);
  uint64_t* diff_ns = (uint64_t*)calloc((size_t)ZR_BENCH_SAMPLE_ITERS, sizeof(uint64_t));
  uint64_t* write_ns = (uint64_t*)calloc((size_t)ZR_BENCH_SAMPLE_ITERS, sizeof(uint64_t));
  uint64_t* bytes = (uint64_t*)calloc((size_t)ZR_BENCH_SAMPLE_ITERS, sizeof(uint64_t));
  if (!damage || !prev_row_hashes || !next_row_hashes || !dirty_rows || !out_buf || !write_buf || !diff_ns ||
      !write_ns || !bytes) {
    zr_fb_release(&fb_a);
    zr_fb_release(&fb_b);
    free(damage);
    free(prev_row_hashes);
    free(next_row_hashes);
    free(dirty_rows);
    free(out_buf);
    free(write_buf);
    free(diff_ns);
    free(write_ns);
    free(bytes);
    return 1;
  }

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_scroll_region = 1u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;

  zr_term_state_t ts;
  memset(&ts, 0, sizeof(ts));

  zr_diff_scratch_t scratch;
  memset(&scratch, 0, sizeof(scratch));
  scratch.prev_row_hashes = prev_row_hashes;
  scratch.next_row_hashes = next_row_hashes;
  scratch.dirty_rows = dirty_rows;
  scratch.row_cap = ZR_BENCH_ROWS;

  const zr_fb_t* src = &fb_a;
  const zr_fb_t* dst = &fb_b;

  const uint32_t total_iters = ZR_BENCH_WARMUP_ITERS + ZR_BENCH_SAMPLE_ITERS;
  for (uint32_t i = 0u; i < total_iters; i++) {
    size_t out_len = 0u;
    zr_term_state_t final_ts;
    zr_diff_stats_t stats;

    const uint64_t diff_t0 = zr_now_ns();
    const zr_result_t rc =
        zr_diff_render_ex(src, dst, &caps, &ts, NULL, &lim, damage, lim.diff_max_damage_rects, &scratch,
                          bench_case->enable_scroll, out_buf, (size_t)ZR_BENCH_OUT_CAP, &out_len, &final_ts, &stats);
    const uint64_t diff_t1 = zr_now_ns();

    if (rc != ZR_OK) {
      zr_fb_release(&fb_a);
      zr_fb_release(&fb_b);
      free(damage);
      free(prev_row_hashes);
      free(next_row_hashes);
      free(dirty_rows);
      free(out_buf);
      free(write_buf);
      free(diff_ns);
      free(write_ns);
      free(bytes);
      return 1;
    }

    const uint64_t write_t0 = zr_now_ns();
    if (out_len != 0u) {
      memcpy(write_buf, out_buf, out_len);
      g_sink ^= (uint64_t)write_buf[out_len - 1u];
    }
    const uint64_t write_t1 = zr_now_ns();

    ts = final_ts;

    if (i >= ZR_BENCH_WARMUP_ITERS) {
      const size_t j = (size_t)(i - ZR_BENCH_WARMUP_ITERS);
      diff_ns[j] = diff_t1 - diff_t0;
      write_ns[j] = write_t1 - write_t0;
      bytes[j] = (uint64_t)stats.bytes_emitted;
    }

    const zr_fb_t* tmp = src;
    src = dst;
    dst = tmp;
  }

  out_metrics->diff_mean_ns = zr_mean_u64(diff_ns, (size_t)ZR_BENCH_SAMPLE_ITERS);
  out_metrics->diff_p95_ns = zr_percentile_ns(diff_ns, (size_t)ZR_BENCH_SAMPLE_ITERS, 95u, 100u);
  out_metrics->diff_p99_ns = zr_percentile_ns(diff_ns, (size_t)ZR_BENCH_SAMPLE_ITERS, 99u, 100u);
  out_metrics->write_mean_ns = zr_mean_u64(write_ns, (size_t)ZR_BENCH_SAMPLE_ITERS);
  out_metrics->write_p95_ns = zr_percentile_ns(write_ns, (size_t)ZR_BENCH_SAMPLE_ITERS, 95u, 100u);
  out_metrics->write_p99_ns = zr_percentile_ns(write_ns, (size_t)ZR_BENCH_SAMPLE_ITERS, 99u, 100u);
  out_metrics->bytes_mean = zr_mean_u64(bytes, (size_t)ZR_BENCH_SAMPLE_ITERS);

  zr_fb_release(&fb_a);
  zr_fb_release(&fb_b);
  free(damage);
  free(prev_row_hashes);
  free(next_row_hashes);
  free(dirty_rows);
  free(out_buf);
  free(write_buf);
  free(diff_ns);
  free(write_ns);
  free(bytes);
  return 0;
}

static uint64_t zr_ns_to_us_rounded(uint64_t ns) {
  return (ns + 500u) / 1000u;
}

int main(void) {
  const zr_bench_case_t cases[] = {
      {"sparse_edits", 0u},
      {"dense_edits", 0u},
      {"scroll_like", 1u},
      {"style_churn", 0u},
  };

  printf("diff_hotpath_bench cols=%u rows=%u warmup=%u samples=%u\n", ZR_BENCH_COLS, ZR_BENCH_ROWS,
         ZR_BENCH_WARMUP_ITERS, ZR_BENCH_SAMPLE_ITERS);
  printf("%-14s %10s %10s %10s %10s %10s %10s %10s\n", "case", "diff_mean", "diff_p95", "diff_p99", "write_mean",
         "write_p95", "write_p99", "bytes_avg");
  printf("%-14s %10s %10s %10s %10s %10s %10s %10s\n", "--------------", "----------", "----------", "----------",
         "----------", "----------", "----------", "----------");

  for (size_t i = 0u; i < (sizeof(cases) / sizeof(cases[0])); i++) {
    zr_bench_metrics_t m;
    memset(&m, 0, sizeof(m));

    if (zr_run_case(&cases[i], &m) != 0) {
      fprintf(stderr, "bench failure case=%s\n", cases[i].name);
      return 1;
    }

    printf("%-14s %10" PRIu64 " %10" PRIu64 " %10" PRIu64 " %10" PRIu64 " %10" PRIu64 " %10" PRIu64 " %10" PRIu64 "\n",
           cases[i].name, zr_ns_to_us_rounded(m.diff_mean_ns), zr_ns_to_us_rounded(m.diff_p95_ns),
           zr_ns_to_us_rounded(m.diff_p99_ns), zr_ns_to_us_rounded(m.write_mean_ns),
           zr_ns_to_us_rounded(m.write_p95_ns), zr_ns_to_us_rounded(m.write_p99_ns), m.bytes_mean);
  }

  if (g_sink == 0xFFFFFFFFFFFFFFFFull) {
    fprintf(stderr, "sink=%" PRIu64 "\n", g_sink);
  }
  return 0;
}
