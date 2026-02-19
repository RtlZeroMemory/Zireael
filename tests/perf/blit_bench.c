/*
  tests/perf/blit_bench.c — Performance sanity benchmark for braille blitter.

  Why: Provides a repeatable local benchmark for the 320x192 -> 160x48
  sub-cell path used by chart/canvas rendering.
*/

#include "core/zr_blit.h"
#include "core/zr_framebuffer.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
  ZR_BLIT_BENCH_SRC_W = 320u,
  ZR_BLIT_BENCH_SRC_H = 192u,
  ZR_BLIT_BENCH_DST_W = 160u,
  ZR_BLIT_BENCH_DST_H = 48u,
  ZR_BLIT_BENCH_WARMUP = 16u,
  ZR_BLIT_BENCH_RUNS = 64u,
};

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

static void zr_fill_pattern(uint8_t* pixels, uint32_t w, uint32_t h) {
  for (uint32_t y = 0u; y < h; y++) {
    for (uint32_t x = 0u; x < w; x++) {
      const size_t o = ((size_t)y * w + x) * 4u;
      pixels[o + 0u] = (uint8_t)((x * 255u) / w);
      pixels[o + 1u] = (uint8_t)((y * 255u) / h);
      pixels[o + 2u] = (uint8_t)(((x + y) * 255u) / (w + h));
      pixels[o + 3u] = 255u;
    }
  }
}

int main(void) {
  uint8_t* pixels = (uint8_t*)malloc((size_t)ZR_BLIT_BENCH_SRC_W * ZR_BLIT_BENCH_SRC_H * 4u);
  if (!pixels) {
    return 1;
  }
  zr_fill_pattern(pixels, ZR_BLIT_BENCH_SRC_W, ZR_BLIT_BENCH_SRC_H);

  zr_blit_input_t in;
  in.pixels = pixels;
  in.px_width = ZR_BLIT_BENCH_SRC_W;
  in.px_height = ZR_BLIT_BENCH_SRC_H;
  in.stride = (uint16_t)(ZR_BLIT_BENCH_SRC_W * 4u);

  zr_fb_t fb;
  zr_fb_painter_t p;
  zr_rect_t stack[2];
  if (zr_fb_init(&fb, ZR_BLIT_BENCH_DST_W, ZR_BLIT_BENCH_DST_H) != ZR_OK) {
    free(pixels);
    return 1;
  }
  (void)zr_fb_clear(&fb, NULL);
  if (zr_fb_painter_begin(&p, &fb, stack, 2u) != ZR_OK) {
    zr_fb_release(&fb);
    free(pixels);
    return 1;
  }

  zr_blit_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.supports_unicode = 1u;
  caps.supports_halfblock = 1u;
  caps.supports_quadrant = 1u;
  caps.supports_braille = 1u;
  caps.supports_sextant = 1u;

  zr_blitter_t effective = ZR_BLIT_ASCII;
  for (uint32_t i = 0u; i < ZR_BLIT_BENCH_WARMUP; i++) {
    (void)zr_blit_dispatch(&p, (zr_rect_t){0, 0, ZR_BLIT_BENCH_DST_W, ZR_BLIT_BENCH_DST_H}, &in, ZR_BLIT_BRAILLE,
                           &caps, &effective);
  }

  uint64_t total_ns = 0u;
  for (uint32_t i = 0u; i < ZR_BLIT_BENCH_RUNS; i++) {
    const uint64_t t0 = zr_now_ns();
    (void)zr_blit_dispatch(&p, (zr_rect_t){0, 0, ZR_BLIT_BENCH_DST_W, ZR_BLIT_BENCH_DST_H}, &in, ZR_BLIT_BRAILLE,
                           &caps, &effective);
    const uint64_t t1 = zr_now_ns();
    total_ns += (t1 - t0);
  }

  const uint64_t mean_ns = total_ns / ZR_BLIT_BENCH_RUNS;
  const double mean_ms = (double)mean_ns / 1000000.0;
  printf("blit_bench mean_ns=%llu mean_ms=%.3f mode=%u\n", (unsigned long long)mean_ns, mean_ms,
         (unsigned)effective);

  zr_fb_release(&fb);
  free(pixels);
  return 0;
}
