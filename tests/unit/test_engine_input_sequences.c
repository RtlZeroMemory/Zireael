/*
  tests/unit/test_engine_input_sequences.c — Engine input parsing (VT sequences).

  Why: Zireael reads raw bytes on POSIX platforms and translates Windows console
  input into a VT-like byte stream. The core byte parser must accept common
  control sequences for arrow keys and SGR mouse so interactive UIs work in
  modern terminals (Rio, WezTerm, Kitty, etc.).
*/

#include "zr_test.h"

#include "core/zr_config.h"
#include "core/zr_engine.h"
#include "core/zr_event.h"

#include "unit/mock_platform.h"

#include "util/zr_bytes.h"

#include <stddef.h>
#include <string.h>

static uint32_t zr_u32le_at(const uint8_t* p) { return zr_load_u32le(p); }

static void zr_drain_initial_resize(zr_test_ctx_t* ctx, zr_engine_t* e) {
  uint8_t out0[128];
  memset(out0, 0, sizeof(out0));
  const int n0 = engine_poll_events(e, 0, out0, (int)sizeof(out0));
  ZR_ASSERT_TRUE(n0 > 0);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out0 + 0u), ZR_EV_MAGIC);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out0 + 4u), ZR_EVENT_BATCH_VERSION_V1);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out0 + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out0 + off_rec0 + 0u), (uint32_t)ZR_EV_RESIZE);
}

ZR_TEST_UNIT(engine_poll_events_parses_csi_arrow_with_params) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  /* Common xterm-style arrow with modifiers: ESC [ 1 ; 5 A */
  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)';', (uint8_t)'5', (uint8_t)'A'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 0u), ZR_EV_MAGIC);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 4u), ZR_EVENT_BATCH_VERSION_V1);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);

  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)ZR_KEY_UP);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_ss3_arrow) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  /* Application cursor mode: ESC O A */
  const uint8_t in[] = {0x1Bu, (uint8_t)'O', (uint8_t)'A'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)ZR_KEY_UP);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_ss3_function_keys) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  /* Common SS3 function keys: F1..F4 as ESC O P/Q/R/S. */
  const uint8_t in[] = {
    0x1Bu, (uint8_t)'O', (uint8_t)'P',
    0x1Bu, (uint8_t)'O', (uint8_t)'Q',
    0x1Bu, (uint8_t)'O', (uint8_t)'R',
    0x1Bu, (uint8_t)'O', (uint8_t)'S',
  };
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 4u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  const size_t rec_bytes = sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t);

  const uint32_t keys[4] = {(uint32_t)ZR_KEY_F1, (uint32_t)ZR_KEY_F2, (uint32_t)ZR_KEY_F3, (uint32_t)ZR_KEY_F4};
  for (size_t i = 0; i < 4; i++) {
    const size_t off_rec = off_rec0 + (rec_bytes * i);
    ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec + 0u), (uint32_t)ZR_EV_KEY);
    const size_t off_payload = off_rec + sizeof(zr_ev_record_header_t);
    ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), keys[i]);
  }

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_csi_tilde_function_keys) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  /* Common CSI ~ function keys: F5..F8. */
  const uint8_t in[] = {
    0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)'5', (uint8_t)'~',
    0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)'7', (uint8_t)'~',
    0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)'8', (uint8_t)'~',
    0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)'9', (uint8_t)'~',
  };
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 4u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  const size_t rec_bytes = sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t);

  const uint32_t keys[4] = {(uint32_t)ZR_KEY_F5, (uint32_t)ZR_KEY_F6, (uint32_t)ZR_KEY_F7, (uint32_t)ZR_KEY_F8};
  for (size_t i = 0; i < 4; i++) {
    const size_t off_rec = off_rec0 + (rec_bytes * i);
    ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec + 0u), (uint32_t)ZR_EV_KEY);
    const size_t off_payload = off_rec + sizeof(zr_ev_record_header_t);
    ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), keys[i]);
  }

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_sgr_mouse_down_up) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  /* Left button down then up at (x=10,y=5) (1-based in SGR). */
  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'<', (uint8_t)'0', (uint8_t)';', (uint8_t)'1', (uint8_t)'0',
                        (uint8_t)';', (uint8_t)'5', (uint8_t)'M', 0x1Bu, (uint8_t)'[', (uint8_t)'<', (uint8_t)'0',
                        (uint8_t)';', (uint8_t)'1', (uint8_t)'0', (uint8_t)';', (uint8_t)'5', (uint8_t)'m'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 2u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_MOUSE);
  const size_t off_payload0 = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload0 + 0u), 9u);  /* x: 10 -> 9 */
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload0 + 4u), 4u);  /* y: 5 -> 4 */
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload0 + 8u), (uint32_t)ZR_MOUSE_DOWN);

  const size_t rec0_bytes = sizeof(zr_ev_record_header_t) + sizeof(zr_ev_mouse_t);
  const size_t off_rec1 = off_rec0 + rec0_bytes;
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec1 + 0u), (uint32_t)ZR_EV_MOUSE);
  const size_t off_payload1 = off_rec1 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload1 + 8u), (uint32_t)ZR_MOUSE_UP);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_sgr_mouse_wheel) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  /* Wheel up at (x=10,y=5): b=64 => wheel up. */
  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'<', (uint8_t)'6', (uint8_t)'4', (uint8_t)';', (uint8_t)'1',
                        (uint8_t)'0', (uint8_t)';', (uint8_t)'5', (uint8_t)'M'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_MOUSE);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 8u), (uint32_t)ZR_MOUSE_WHEEL);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 20u), 0u);          /* wheel_x */
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 24u), 1u);          /* wheel_y */

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_sgr_motion_without_buttons_as_move) {
  mock_plat_reset();
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  /*
    Any-event motion with no buttons pressed:
      - motion bit set (32)
      - base=3 (no buttons)
      => b=35
  */
  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'<', (uint8_t)'3', (uint8_t)'5', (uint8_t)';', (uint8_t)'1',
                        (uint8_t)'0', (uint8_t)';', (uint8_t)'5', (uint8_t)'M'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_MOUSE);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 8u), (uint32_t)ZR_MOUSE_MOVE);

  engine_destroy(e);
}
