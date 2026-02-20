/*
  tests/perf/image_encode_bench.c — Deterministic microbench for image encoders.

  Why: Tracks protocol encoding cost (base64, Kitty, Sixel, iTerm2) so image
  pipeline changes can be evaluated quickly during development.
*/

#include "core/zr_base64.h"
#include "core/zr_image.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
  ZR_IMG_BENCH_WARMUP_ITERS = 16u,
  ZR_IMG_BENCH_SAMPLE_ITERS = 128u,
  ZR_IMG_BENCH_B64_RAW_BYTES = 65536u,
  ZR_IMG_BENCH_KITTY_W = 64u,
  ZR_IMG_BENCH_KITTY_H = 64u,
  ZR_IMG_BENCH_SIXEL_W = 32u,
  ZR_IMG_BENCH_SIXEL_H = 24u,
  ZR_IMG_BENCH_ITERM2_W = 16u,
  ZR_IMG_BENCH_ITERM2_H = 16u,
  ZR_IMG_BENCH_OUT_CAP = 512u * 1024u,
  ZR_IMG_BENCH_ARENA_INIT = 256u * 1024u,
  ZR_IMG_BENCH_ARENA_MAX = 2u * 1024u * 1024u,
};

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

static uint64_t zr_mean_ns(uint64_t total_ns, uint32_t samples) {
  if (samples == 0u) {
    return 0u;
  }
  return total_ns / (uint64_t)samples;
}

static void zr_fill_pattern(uint8_t* bytes, size_t len, uint8_t seed) {
  size_t i = 0u;
  if (!bytes) {
    return;
  }
  for (i = 0u; i < len; i++) {
    bytes[i] = (uint8_t)(seed + (uint8_t)(i * 31u));
  }
}

static int zr_bench_base64(void) {
  uint8_t in[ZR_IMG_BENCH_B64_RAW_BYTES];
  uint8_t out[ZR_IMG_BENCH_B64_RAW_BYTES * 2u];
  uint64_t total_ns = 0u;
  size_t out_len = 0u;

  zr_fill_pattern(in, sizeof(in), 7u);

  for (uint32_t i = 0u; i < ZR_IMG_BENCH_WARMUP_ITERS; i++) {
    out_len = 0u;
    if (zr_base64_encode(in, sizeof(in), out, sizeof(out), &out_len) != ZR_OK) {
      return 1;
    }
    g_sink += out_len;
  }

  for (uint32_t i = 0u; i < ZR_IMG_BENCH_SAMPLE_ITERS; i++) {
    const uint64_t t0 = zr_now_ns();
    out_len = 0u;
    if (zr_base64_encode(in, sizeof(in), out, sizeof(out), &out_len) != ZR_OK) {
      return 1;
    }
    const uint64_t t1 = zr_now_ns();
    total_ns += (t1 - t0);
    g_sink += out_len;
  }

  printf("image_encode_bench case=base64 raw_bytes=%u mean_ns=%" PRIu64 "\n", (unsigned)sizeof(in),
         zr_mean_ns(total_ns, ZR_IMG_BENCH_SAMPLE_ITERS));
  return 0;
}

static int zr_bench_kitty(void) {
  uint8_t rgba[ZR_IMG_BENCH_KITTY_W * ZR_IMG_BENCH_KITTY_H * ZR_IMAGE_RGBA_BYTES_PER_PIXEL];
  uint8_t out[ZR_IMG_BENCH_OUT_CAP];
  zr_sb_t sb;
  uint64_t total_ns = 0u;

  zr_fill_pattern(rgba, sizeof(rgba), 11u);
  zr_sb_init(&sb, out, sizeof(out));

  for (uint32_t i = 0u; i < ZR_IMG_BENCH_WARMUP_ITERS; i++) {
    zr_sb_reset(&sb);
    if (zr_image_kitty_emit_transmit_rgba(&sb, 100u + i, rgba, ZR_IMG_BENCH_KITTY_W, ZR_IMG_BENCH_KITTY_H,
                                          ZR_IMG_BENCH_KITTY_W / 2u, ZR_IMG_BENCH_KITTY_H / 2u) != ZR_OK) {
      return 1;
    }
    g_sink += sb.len;
  }

  for (uint32_t i = 0u; i < ZR_IMG_BENCH_SAMPLE_ITERS; i++) {
    const uint64_t t0 = zr_now_ns();
    zr_sb_reset(&sb);
    if (zr_image_kitty_emit_transmit_rgba(&sb, 1000u + i, rgba, ZR_IMG_BENCH_KITTY_W, ZR_IMG_BENCH_KITTY_H,
                                          ZR_IMG_BENCH_KITTY_W / 2u, ZR_IMG_BENCH_KITTY_H / 2u) != ZR_OK) {
      return 1;
    }
    const uint64_t t1 = zr_now_ns();
    total_ns += (t1 - t0);
    g_sink += sb.len;
  }

  printf("image_encode_bench case=kitty rgba=%ux%u mean_ns=%" PRIu64 "\n", (unsigned)ZR_IMG_BENCH_KITTY_W,
         (unsigned)ZR_IMG_BENCH_KITTY_H, zr_mean_ns(total_ns, ZR_IMG_BENCH_SAMPLE_ITERS));
  return 0;
}

static int zr_bench_sixel(void) {
  uint8_t rgba[ZR_IMG_BENCH_SIXEL_W * ZR_IMG_BENCH_SIXEL_H * ZR_IMAGE_RGBA_BYTES_PER_PIXEL];
  uint8_t out[ZR_IMG_BENCH_OUT_CAP];
  zr_sb_t sb;
  zr_arena_t arena;
  uint64_t total_ns = 0u;

  zr_fill_pattern(rgba, sizeof(rgba), 13u);
  zr_sb_init(&sb, out, sizeof(out));
  if (zr_arena_init(&arena, ZR_IMG_BENCH_ARENA_INIT, ZR_IMG_BENCH_ARENA_MAX) != ZR_OK) {
    return 1;
  }

  for (uint32_t i = 0u; i < ZR_IMG_BENCH_WARMUP_ITERS; i++) {
    zr_sb_reset(&sb);
    zr_arena_reset(&arena);
    if (zr_image_sixel_emit_rgba(&sb, &arena, rgba, ZR_IMG_BENCH_SIXEL_W, ZR_IMG_BENCH_SIXEL_H, 0u, 0u) != ZR_OK) {
      zr_arena_release(&arena);
      return 1;
    }
    g_sink += sb.len;
  }

  for (uint32_t i = 0u; i < ZR_IMG_BENCH_SAMPLE_ITERS; i++) {
    const uint64_t t0 = zr_now_ns();
    zr_sb_reset(&sb);
    zr_arena_reset(&arena);
    if (zr_image_sixel_emit_rgba(&sb, &arena, rgba, ZR_IMG_BENCH_SIXEL_W, ZR_IMG_BENCH_SIXEL_H, 0u, 0u) != ZR_OK) {
      zr_arena_release(&arena);
      return 1;
    }
    const uint64_t t1 = zr_now_ns();
    total_ns += (t1 - t0);
    g_sink += sb.len;
  }

  zr_arena_release(&arena);

  printf("image_encode_bench case=sixel rgba=%ux%u mean_ns=%" PRIu64 "\n", (unsigned)ZR_IMG_BENCH_SIXEL_W,
         (unsigned)ZR_IMG_BENCH_SIXEL_H, zr_mean_ns(total_ns, ZR_IMG_BENCH_SAMPLE_ITERS));
  return 0;
}

static int zr_bench_iterm2_rgba(void) {
  uint8_t rgba[ZR_IMG_BENCH_ITERM2_W * ZR_IMG_BENCH_ITERM2_H * ZR_IMAGE_RGBA_BYTES_PER_PIXEL];
  uint8_t out[ZR_IMG_BENCH_OUT_CAP];
  zr_sb_t sb;
  zr_arena_t arena;
  uint64_t total_ns = 0u;

  zr_fill_pattern(rgba, sizeof(rgba), 17u);
  zr_sb_init(&sb, out, sizeof(out));
  if (zr_arena_init(&arena, ZR_IMG_BENCH_ARENA_INIT, ZR_IMG_BENCH_ARENA_MAX) != ZR_OK) {
    return 1;
  }

  for (uint32_t i = 0u; i < ZR_IMG_BENCH_WARMUP_ITERS; i++) {
    zr_sb_reset(&sb);
    zr_arena_reset(&arena);
    if (zr_image_iterm2_emit_rgba(&sb, &arena, rgba, ZR_IMG_BENCH_ITERM2_W, ZR_IMG_BENCH_ITERM2_H, 0u, 0u,
                                  ZR_IMG_BENCH_ITERM2_W / 8u, ZR_IMG_BENCH_ITERM2_H / 16u) != ZR_OK) {
      zr_arena_release(&arena);
      return 1;
    }
    g_sink += sb.len;
  }

  for (uint32_t i = 0u; i < ZR_IMG_BENCH_SAMPLE_ITERS; i++) {
    const uint64_t t0 = zr_now_ns();
    zr_sb_reset(&sb);
    zr_arena_reset(&arena);
    if (zr_image_iterm2_emit_rgba(&sb, &arena, rgba, ZR_IMG_BENCH_ITERM2_W, ZR_IMG_BENCH_ITERM2_H, 0u, 0u,
                                  ZR_IMG_BENCH_ITERM2_W / 8u, ZR_IMG_BENCH_ITERM2_H / 16u) != ZR_OK) {
      zr_arena_release(&arena);
      return 1;
    }
    const uint64_t t1 = zr_now_ns();
    total_ns += (t1 - t0);
    g_sink += sb.len;
  }

  zr_arena_release(&arena);

  printf("image_encode_bench case=iterm2_rgba rgba=%ux%u mean_ns=%" PRIu64 "\n", (unsigned)ZR_IMG_BENCH_ITERM2_W,
         (unsigned)ZR_IMG_BENCH_ITERM2_H, zr_mean_ns(total_ns, ZR_IMG_BENCH_SAMPLE_ITERS));
  return 0;
}

int main(void) {
  int rc = 0;

  rc |= zr_bench_base64();
  rc |= zr_bench_kitty();
  rc |= zr_bench_sixel();
  rc |= zr_bench_iterm2_rgba();

  printf("image_encode_bench sink=%" PRIu64 "\n", g_sink);
  return rc == 0 ? 0 : 1;
}
