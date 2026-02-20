/*
  tests/unit/test_image_lifecycle.c — Unit tests for image frame lifecycle.

  Why: Engine-owned staging/caching for DRAW_IMAGE must preserve no-surprises
  ownership and deterministic cleanup across presents.
*/

#include "zr_test.h"

#include "core/zr_image.h"

#include <stdint.h>
#include <string.h>

ZR_TEST_UNIT(image_lifecycle_frame_push_copy_and_swap_roundtrip) {
  zr_image_frame_t a;
  zr_image_frame_t b;
  const uint8_t blob_a0[4] = {1u, 2u, 3u, 4u};
  const uint8_t blob_a1[4] = {5u, 6u, 7u, 8u};
  const uint8_t blob_b0[4] = {9u, 10u, 11u, 12u};

  zr_image_cmd_t cmd_a0;
  zr_image_cmd_t cmd_a1;
  zr_image_cmd_t cmd_b0;

  memset(&cmd_a0, 0, sizeof(cmd_a0));
  cmd_a0.blob_len = 4u;
  cmd_a0.px_width = 1u;
  cmd_a0.px_height = 1u;

  cmd_a1 = cmd_a0;
  cmd_b0 = cmd_a0;

  zr_image_frame_init(&a);
  zr_image_frame_init(&b);

  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(&a, &cmd_a0, blob_a0), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(&a, &cmd_a1, blob_a1), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(&b, &cmd_b0, blob_b0), ZR_OK);

  ZR_ASSERT_EQ_U32(a.cmds_len, 2u);
  ZR_ASSERT_EQ_U32(a.blob_len, 8u);
  ZR_ASSERT_EQ_U32(a.cmds[0].blob_off, 0u);
  ZR_ASSERT_EQ_U32(a.cmds[1].blob_off, 4u);
  ZR_ASSERT_MEMEQ(a.blob_bytes + 0u, blob_a0, 4u);
  ZR_ASSERT_MEMEQ(a.blob_bytes + 4u, blob_a1, 4u);

  ZR_ASSERT_EQ_U32(b.cmds_len, 1u);
  ZR_ASSERT_EQ_U32(b.blob_len, 4u);
  ZR_ASSERT_MEMEQ(b.blob_bytes, blob_b0, 4u);

  zr_image_frame_swap(&a, &b);

  ZR_ASSERT_EQ_U32(a.cmds_len, 1u);
  ZR_ASSERT_EQ_U32(a.blob_len, 4u);
  ZR_ASSERT_MEMEQ(a.blob_bytes, blob_b0, 4u);

  ZR_ASSERT_EQ_U32(b.cmds_len, 2u);
  ZR_ASSERT_EQ_U32(b.blob_len, 8u);
  ZR_ASSERT_MEMEQ(b.blob_bytes + 0u, blob_a0, 4u);
  ZR_ASSERT_MEMEQ(b.blob_bytes + 4u, blob_a1, 4u);

  zr_image_frame_release(&a);
  zr_image_frame_release(&b);
}

ZR_TEST_UNIT(image_lifecycle_frame_push_copy_rejects_invalid_arguments) {
  zr_image_frame_t frame;
  zr_image_cmd_t cmd;
  const uint8_t blob[4] = {1u, 2u, 3u, 4u};

  zr_image_frame_init(&frame);
  memset(&cmd, 0, sizeof(cmd));
  cmd.blob_len = 4u;

  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(NULL, &cmd, blob), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(&frame, NULL, blob), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(&frame, &cmd, NULL), ZR_ERR_INVALID_ARGUMENT);

  cmd.blob_len = 0u;
  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(&frame, &cmd, NULL), ZR_OK);

  zr_image_frame_release(&frame);
}

ZR_TEST_UNIT(image_lifecycle_emit_frame_kitty_transmit_then_cleanup_delete) {
  zr_image_frame_t frame_a;
  zr_image_frame_t frame_b;
  zr_image_state_t state;
  zr_arena_t arena;
  zr_image_emit_ctx_t ctx_emit;
  zr_sb_t sb;
  uint8_t out[2048];
  const uint8_t rgba[4] = {1u, 2u, 3u, 255u};

  zr_image_cmd_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.dst_col = 0u;
  cmd.dst_row = 0u;
  cmd.dst_cols = 1u;
  cmd.dst_rows = 1u;
  cmd.px_width = 1u;
  cmd.px_height = 1u;
  cmd.blob_len = 4u;
  cmd.image_id = 42u;
  cmd.format = (uint8_t)ZR_IMAGE_FORMAT_RGBA;
  cmd.protocol = (uint8_t)ZR_IMG_PROTO_KITTY;
  cmd.fit_mode = (uint8_t)ZR_IMAGE_FIT_FILL;

  zr_image_frame_init(&frame_a);
  zr_image_frame_init(&frame_b);
  zr_image_state_init(&state);
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);
  zr_sb_init(&sb, out, sizeof(out));

  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(&frame_a, &cmd, rgba), ZR_OK);

  memset(&ctx_emit, 0, sizeof(ctx_emit));
  ctx_emit.frame = &frame_a;
  ctx_emit.arena = &arena;
  ctx_emit.state = &state;
  ctx_emit.out = &sb;

  ZR_ASSERT_EQ_U32(zr_image_emit_frame(&ctx_emit), ZR_OK);

  {
    static const uint8_t expected_first[] = "\x1b_Ga=t,f=32,s=1,v=1,i=1,m=0;AQID/w==\x1b\\"
                                            "\x1b[1;1H\x1b_Ga=p,i=1,c=1,r=1,z=0\x1b\\";
    ZR_ASSERT_TRUE(sb.len == (sizeof(expected_first) - 1u));
    ZR_ASSERT_MEMEQ(out, expected_first, sizeof(expected_first) - 1u);
  }

  ZR_ASSERT_EQ_U32(state.slot_count, 1u);
  ZR_ASSERT_EQ_U32(state.slots[0].transmitted, 1u);
  ZR_ASSERT_EQ_U32(state.slots[0].placed_this_frame, 1u);

  zr_sb_reset(&sb);
  zr_arena_reset(&arena);
  ctx_emit.frame = &frame_b;
  ZR_ASSERT_EQ_U32(zr_image_emit_frame(&ctx_emit), ZR_OK);

  {
    static const uint8_t expected_second[] = "\x1b_Ga=d,d=i,i=1\x1b\\";
    ZR_ASSERT_TRUE(sb.len == (sizeof(expected_second) - 1u));
    ZR_ASSERT_MEMEQ(out, expected_second, sizeof(expected_second) - 1u);
  }

  ZR_ASSERT_EQ_U32(state.slots[0].transmitted, 0u);
  ZR_ASSERT_EQ_U32(state.slots[0].kitty_id, 0u);

  zr_arena_release(&arena);
  zr_image_frame_release(&frame_a);
  zr_image_frame_release(&frame_b);
}

ZR_TEST_UNIT(image_lifecycle_emit_frame_kitty_retransmits_when_dims_change_for_same_id_hash) {
  zr_image_frame_t frame_a;
  zr_image_frame_t frame_b;
  zr_image_state_t state;
  zr_arena_t arena;
  zr_image_emit_ctx_t ctx_emit;
  zr_sb_t sb;
  uint8_t out[4096];
  static const uint8_t rgba[16] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u};
  zr_image_cmd_t cmd_a;
  zr_image_cmd_t cmd_b;

  memset(&cmd_a, 0, sizeof(cmd_a));
  cmd_a.dst_col = 0u;
  cmd_a.dst_row = 0u;
  cmd_a.dst_cols = 1u;
  cmd_a.dst_rows = 1u;
  cmd_a.px_width = 2u;
  cmd_a.px_height = 2u;
  cmd_a.blob_len = 16u;
  cmd_a.image_id = 42u;
  cmd_a.format = (uint8_t)ZR_IMAGE_FORMAT_RGBA;
  cmd_a.protocol = (uint8_t)ZR_IMG_PROTO_KITTY;
  cmd_a.fit_mode = (uint8_t)ZR_IMAGE_FIT_FILL;
  cmd_b = cmd_a;
  cmd_b.px_width = 1u;
  cmd_b.px_height = 4u;

  zr_image_frame_init(&frame_a);
  zr_image_frame_init(&frame_b);
  zr_image_state_init(&state);
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);
  zr_sb_init(&sb, out, sizeof(out));

  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(&frame_a, &cmd_a, rgba), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(&frame_b, &cmd_b, rgba), ZR_OK);

  memset(&ctx_emit, 0, sizeof(ctx_emit));
  ctx_emit.arena = &arena;
  ctx_emit.state = &state;
  ctx_emit.out = &sb;

  ctx_emit.frame = &frame_a;
  ZR_ASSERT_EQ_U32(zr_image_emit_frame(&ctx_emit), ZR_OK);
  {
    static const uint8_t expected_first[] = "\x1b_Ga=t,f=32,s=2,v=2,i=1,m=0;AQIDBAUGBwgJCgsMDQ4PEA==\x1b\\"
                                            "\x1b[1;1H\x1b_Ga=p,i=1,c=1,r=1,z=0\x1b\\";
    ZR_ASSERT_TRUE(sb.len == (sizeof(expected_first) - 1u));
    ZR_ASSERT_MEMEQ(out, expected_first, sizeof(expected_first) - 1u);
  }

  zr_sb_reset(&sb);
  zr_arena_reset(&arena);
  ctx_emit.frame = &frame_b;
  ZR_ASSERT_EQ_U32(zr_image_emit_frame(&ctx_emit), ZR_OK);
  {
    static const uint8_t expected_second[] = "\x1b_Ga=t,f=32,s=1,v=4,i=2,m=0;AQIDBAUGBwgJCgsMDQ4PEA==\x1b\\"
                                             "\x1b[1;1H\x1b_Ga=p,i=2,c=1,r=1,z=0\x1b\\"
                                             "\x1b_Ga=d,d=i,i=1\x1b\\";
    ZR_ASSERT_TRUE(sb.len == (sizeof(expected_second) - 1u));
    ZR_ASSERT_MEMEQ(out, expected_second, sizeof(expected_second) - 1u);
  }

  ZR_ASSERT_EQ_U32(state.slot_count, 2u);
  ZR_ASSERT_EQ_U32(state.slots[0].transmitted, 0u);
  ZR_ASSERT_EQ_U32(state.slots[1].transmitted, 1u);
  ZR_ASSERT_EQ_U32(state.slots[1].image_id, 42u);
  ZR_ASSERT_EQ_U32(state.slots[1].px_width, 1u);
  ZR_ASSERT_EQ_U32(state.slots[1].px_height, 4u);

  zr_arena_release(&arena);
  zr_image_frame_release(&frame_a);
  zr_image_frame_release(&frame_b);
}

ZR_TEST_UNIT(image_lifecycle_emit_frame_rejects_out_of_bounds_blob_slice) {
  zr_image_frame_t frame;
  zr_image_state_t state;
  zr_arena_t arena;
  zr_image_emit_ctx_t ctx_emit;
  zr_sb_t sb;
  uint8_t out[256];
  const uint8_t rgba[4] = {1u, 2u, 3u, 255u};
  zr_image_cmd_t cmd;

  memset(&cmd, 0, sizeof(cmd));
  cmd.dst_col = 0u;
  cmd.dst_row = 0u;
  cmd.dst_cols = 1u;
  cmd.dst_rows = 1u;
  cmd.px_width = 1u;
  cmd.px_height = 1u;
  cmd.blob_len = 4u;
  cmd.image_id = 7u;
  cmd.format = (uint8_t)ZR_IMAGE_FORMAT_RGBA;
  cmd.protocol = (uint8_t)ZR_IMG_PROTO_KITTY;
  cmd.fit_mode = (uint8_t)ZR_IMAGE_FIT_FILL;

  zr_image_frame_init(&frame);
  zr_image_state_init(&state);
  ZR_ASSERT_EQ_U32(zr_arena_init(&arena, 4096u, 65536u), ZR_OK);
  zr_sb_init(&sb, out, sizeof(out));
  ZR_ASSERT_EQ_U32(zr_image_frame_push_copy(&frame, &cmd, rgba), ZR_OK);

  frame.cmds[0].blob_off = frame.blob_len;

  memset(&ctx_emit, 0, sizeof(ctx_emit));
  ctx_emit.frame = &frame;
  ctx_emit.arena = &arena;
  ctx_emit.state = &state;
  ctx_emit.out = &sb;

  ZR_ASSERT_EQ_U32(zr_image_emit_frame(&ctx_emit), ZR_ERR_INVALID_ARGUMENT);
  ZR_ASSERT_TRUE(sb.len == 0u);

  zr_arena_release(&arena);
  zr_image_frame_release(&frame);
}
