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

static uint32_t zr_u32le_at(const uint8_t* p) {
  return zr_load_u32le(p);
}

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

static bool zr_batch_contains_record_type(const uint8_t* bytes, size_t len, uint32_t want_type) {
  if (!bytes || len < sizeof(zr_evbatch_header_t)) {
    return false;
  }
  if (zr_u32le_at(bytes + 0u) != ZR_EV_MAGIC) {
    return false;
  }

  const uint32_t event_count = zr_u32le_at(bytes + 12u);
  size_t off = sizeof(zr_evbatch_header_t);
  for (uint32_t i = 0u; i < event_count; i++) {
    if ((off + sizeof(zr_ev_record_header_t)) > len) {
      return false;
    }

    const uint32_t rec_type = zr_u32le_at(bytes + off + 0u);
    const uint32_t rec_size = zr_u32le_at(bytes + off + 4u);
    if (rec_type == want_type) {
      return true;
    }
    if (rec_size < (uint32_t)sizeof(zr_ev_record_header_t)) {
      return false;
    }
    if ((rec_size & 3u) != 0u) {
      return false;
    }
    if ((off + (size_t)rec_size) > len) {
      return false;
    }
    off += (size_t)rec_size;
  }
  return false;
}

/*
  Assert deterministic extended-sequence fallback:
    - first event is Escape key down
    - remaining events are literal ASCII bytes as text scalars
*/
static void zr_assert_escape_then_ascii_text_events(zr_test_ctx_t* ctx, const uint8_t* out, size_t len,
                                                    const char* ascii_tail) {
  ZR_ASSERT_TRUE(out != NULL);
  ZR_ASSERT_TRUE(ascii_tail != NULL);
  ZR_ASSERT_TRUE(len >= sizeof(zr_evbatch_header_t));

  const size_t tail_len = strlen(ascii_tail);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), (uint32_t)(1u + tail_len));

  size_t off = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_TRUE((off + sizeof(zr_ev_record_header_t)) <= len);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off + 0u), (uint32_t)ZR_EV_KEY);

  const uint32_t rec0_size = zr_u32le_at(out + off + 4u);
  ZR_ASSERT_TRUE(rec0_size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t)));
  ZR_ASSERT_TRUE((off + (size_t)rec0_size) <= len);

  const size_t off_key_payload = off + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_key_payload + 0u), (uint32_t)ZR_KEY_ESCAPE);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_key_payload + 4u), 0u);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_key_payload + 8u), (uint32_t)ZR_KEY_ACTION_DOWN);
  off += (size_t)rec0_size;

  for (size_t i = 0u; i < tail_len; i++) {
    ZR_ASSERT_TRUE((off + sizeof(zr_ev_record_header_t)) <= len);
    ZR_ASSERT_EQ_U32(zr_u32le_at(out + off + 0u), (uint32_t)ZR_EV_TEXT);

    const uint32_t rec_size = zr_u32le_at(out + off + 4u);
    ZR_ASSERT_TRUE(rec_size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_text_t)));
    ZR_ASSERT_TRUE((off + (size_t)rec_size) <= len);

    const size_t off_payload = off + sizeof(zr_ev_record_header_t);
    const uint32_t want_cp = (uint32_t)(uint8_t)ascii_tail[i];
    ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), want_cp);
    off += (size_t)rec_size;
  }
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
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 4u), (uint32_t)ZR_MOD_CTRL);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 8u), (uint32_t)ZR_KEY_ACTION_DOWN);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_csi_shift_tab) {
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

  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'Z'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 4u), (uint32_t)ZR_MOD_SHIFT);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 8u), (uint32_t)ZR_KEY_ACTION_DOWN);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_csi_focus_in_out) {
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

  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'I', 0x1Bu, (uint8_t)'[', (uint8_t)'O'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 2u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  const size_t rec_bytes = sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload0 = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload0 + 0u), (uint32_t)ZR_KEY_FOCUS_IN);

  const size_t off_rec1 = off_rec0 + rec_bytes;
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec1 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload1 = off_rec1 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload1 + 0u), (uint32_t)ZR_KEY_FOCUS_OUT);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_csi_u_tab_with_ctrl) {
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

  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'9', (uint8_t)';', (uint8_t)'5', (uint8_t)'u'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 4u), (uint32_t)ZR_MOD_CTRL);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 8u), (uint32_t)ZR_KEY_ACTION_DOWN);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_modify_other_keys_alt_text) {
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
    xterm modifyOtherKeys: CSI 27;3;97~ means Alt+'a'.
    Parser normalizes this as Escape key + text scalar 'a'.
  */
  const uint8_t in[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)'7', (uint8_t)';',
                        (uint8_t)'3', (uint8_t)';', (uint8_t)'9', (uint8_t)'7', (uint8_t)'~'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 2u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload0 = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload0 + 0u), (uint32_t)ZR_KEY_ESCAPE);

  const size_t off_rec1 = off_rec0 + sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec1 + 0u), (uint32_t)ZR_EV_TEXT);
  const size_t off_payload1 = off_rec1 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload1 + 0u), (uint32_t)'a');

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_split_csi_focus_in) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in0[] = {0x1Bu, (uint8_t)'['};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in0, sizeof(in0)), ZR_OK);

  uint8_t out0[128];
  memset(out0, 0, sizeof(out0));
  ZR_ASSERT_EQ_U32(engine_poll_events(e, 0, out0, (int)sizeof(out0)), 0u);

  const uint8_t in1[] = {(uint8_t)'I'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in1, sizeof(in1)), ZR_OK);

  uint8_t out1[128];
  memset(out1, 0, sizeof(out1));
  const int n = engine_poll_events(e, 0, out1, (int)sizeof(out1));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_payload + 0u), (uint32_t)ZR_KEY_FOCUS_IN);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_split_csi_focus_out) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in0[] = {0x1Bu, (uint8_t)'['};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in0, sizeof(in0)), ZR_OK);

  uint8_t out0[128];
  memset(out0, 0, sizeof(out0));
  ZR_ASSERT_EQ_U32(engine_poll_events(e, 0, out0, (int)sizeof(out0)), 0u);

  const uint8_t in1[] = {(uint8_t)'O'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in1, sizeof(in1)), ZR_OK);

  uint8_t out1[128];
  memset(out1, 0, sizeof(out1));
  const int n = engine_poll_events(e, 0, out1, (int)sizeof(out1));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_payload + 0u), (uint32_t)ZR_KEY_FOCUS_OUT);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_split_csi_u_across_polls) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in0[] = {0x1Bu, (uint8_t)'[', (uint8_t)'9', (uint8_t)';', (uint8_t)'5'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in0, sizeof(in0)), ZR_OK);

  uint8_t out0[128];
  memset(out0, 0, sizeof(out0));
  ZR_ASSERT_EQ_U32(engine_poll_events(e, 0, out0, (int)sizeof(out0)), 0u);

  const uint8_t in1[] = {(uint8_t)'u'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in1, sizeof(in1)), ZR_OK);

  uint8_t out1[128];
  memset(out1, 0, sizeof(out1));
  const int n = engine_poll_events(e, 0, out1, (int)sizeof(out1));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_payload + 0u), (uint32_t)ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_payload + 4u), (uint32_t)ZR_MOD_CTRL);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_csi_u_alt_text) {
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

  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'9', (uint8_t)'7', (uint8_t)';', (uint8_t)'3', (uint8_t)'u'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 2u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload0 = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload0 + 0u), (uint32_t)ZR_KEY_ESCAPE);

  const size_t off_rec1 = off_rec0 + sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec1 + 0u), (uint32_t)ZR_EV_TEXT);
  const size_t off_payload1 = off_rec1 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload1 + 0u), (uint32_t)'a');

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_csi_u_enter_with_ctrl) {
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

  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'1', (uint8_t)'3', (uint8_t)';', (uint8_t)'5', (uint8_t)'u'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)ZR_KEY_ENTER);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 4u), (uint32_t)ZR_MOD_CTRL);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 8u), (uint32_t)ZR_KEY_ACTION_DOWN);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_csi_u_with_extra_param) {
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

  const uint8_t in[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'9', (uint8_t)';', (uint8_t)'5',
                        (uint8_t)';', (uint8_t)'1', (uint8_t)'2', (uint8_t)'3', (uint8_t)'u'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 4u), (uint32_t)ZR_MOD_CTRL);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_csi_u_invalid_scalar_with_mods_as_unknown_key) {
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

  const uint8_t in[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'1', (uint8_t)'1', (uint8_t)'1', (uint8_t)'4',
                        (uint8_t)'1', (uint8_t)'1', (uint8_t)'2', (uint8_t)';', (uint8_t)'5', (uint8_t)'u'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)ZR_KEY_UNKNOWN);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 4u), (uint32_t)ZR_MOD_CTRL);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_modify_other_keys_ctrl_tab) {
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

  const uint8_t in[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)'7', (uint8_t)';',
                        (uint8_t)'5', (uint8_t)';', (uint8_t)'9', (uint8_t)'~'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 4u), (uint32_t)ZR_MOD_CTRL);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_modify_other_keys_meta_text) {
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

  const uint8_t in[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)'7', (uint8_t)';',
                        (uint8_t)'9', (uint8_t)';', (uint8_t)'9', (uint8_t)'7', (uint8_t)'~'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 2u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload0 = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload0 + 0u), (uint32_t)ZR_KEY_ESCAPE);

  const size_t off_rec1 = off_rec0 + sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec1 + 0u), (uint32_t)ZR_EV_TEXT);
  const size_t off_payload1 = off_rec1 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload1 + 0u), (uint32_t)'a');

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_modify_other_keys_with_extra_param) {
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

  const uint8_t in[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)'7', (uint8_t)';', (uint8_t)'5',
                        (uint8_t)';', (uint8_t)'9', (uint8_t)';', (uint8_t)'7', (uint8_t)'7', (uint8_t)'~'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 4u), (uint32_t)ZR_MOD_CTRL);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_parses_split_modify_other_keys_alt_text) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in0[] = {
      0x1Bu, (uint8_t)'[', (uint8_t)'2', (uint8_t)'7', (uint8_t)';', (uint8_t)'3', (uint8_t)';',
  };
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in0, sizeof(in0)), ZR_OK);

  uint8_t out0[128];
  memset(out0, 0, sizeof(out0));
  ZR_ASSERT_EQ_U32(engine_poll_events(e, 0, out0, (int)sizeof(out0)), 0u);

  const uint8_t in1[] = {(uint8_t)'9', (uint8_t)'7', (uint8_t)'~'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in1, sizeof(in1)), ZR_OK);

  uint8_t out1[256];
  memset(out1, 0, sizeof(out1));
  const int n = engine_poll_events(e, 0, out1, (int)sizeof(out1));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + 12u), 2u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload0 = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_payload0 + 0u), (uint32_t)ZR_KEY_ESCAPE);

  const size_t off_rec1 = off_rec0 + sizeof(zr_ev_record_header_t) + sizeof(zr_ev_key_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_rec1 + 0u), (uint32_t)ZR_EV_TEXT);
  const size_t off_payload1 = off_rec1 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_payload1 + 0u), (uint32_t)'a');

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_falls_back_on_malformed_csi_u_sequence) {
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

  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'9', (uint8_t)';', (uint8_t)'x', (uint8_t)'u'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[512];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  zr_assert_escape_then_ascii_text_events(ctx, out, (size_t)n, "[9;xu");

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_falls_back_on_csi_u_invalid_scalar_without_mods) {
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

  const uint8_t in[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'1', (uint8_t)'1', (uint8_t)'1',
                        (uint8_t)'4', (uint8_t)'1', (uint8_t)'1', (uint8_t)'2', (uint8_t)'u'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[512];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  zr_assert_escape_then_ascii_text_events(ctx, out, (size_t)n, "[1114112u");

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_falls_back_on_malformed_modify_other_keys_sequence) {
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

  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'2', (uint8_t)'7', (uint8_t)';', (uint8_t)'3', (uint8_t)'~'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[512];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  zr_assert_escape_then_ascii_text_events(ctx, out, (size_t)n, "[27;3~");

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_flushes_incomplete_csi_u_on_idle_poll) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'9', (uint8_t)';'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out0[128];
  memset(out0, 0, sizeof(out0));
  ZR_ASSERT_EQ_U32(engine_poll_events(e, 0, out0, (int)sizeof(out0)), 0u);

  uint8_t out1[256];
  memset(out1, 0, sizeof(out1));
  const int n = engine_poll_events(e, 0, out1, (int)sizeof(out1));
  ZR_ASSERT_TRUE(n > 0);

  zr_assert_escape_then_ascii_text_events(ctx, out1, (size_t)n, "[9;");

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_emits_text_scalars_from_utf8_and_invalid_bytes) {
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

  /* U+20AC (Euro sign) followed by invalid byte 0xFF -> U+FFFD replacement. */
  const uint8_t in[] = {0xE2u, 0x82u, 0xACu, 0xFFu};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  const uint32_t event_count = zr_u32le_at(out + 12u);
  ZR_ASSERT_TRUE(event_count >= 2u);

  size_t off = sizeof(zr_evbatch_header_t);
  uint32_t text_seen = 0u;
  uint32_t cps[2] = {0u, 0u};

  for (uint32_t i = 0u; i < event_count; i++) {
    ZR_ASSERT_TRUE((off + sizeof(zr_ev_record_header_t)) <= (size_t)n);

    const uint32_t rec_type = zr_u32le_at(out + off + 0u);
    const uint32_t rec_size = zr_u32le_at(out + off + 4u);
    ZR_ASSERT_TRUE(rec_size >= (uint32_t)sizeof(zr_ev_record_header_t));
    ZR_ASSERT_TRUE((off + (size_t)rec_size) <= (size_t)n);

    if (rec_type == (uint32_t)ZR_EV_TEXT) {
      ZR_ASSERT_TRUE(rec_size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_text_t)));
      if (text_seen < 2u) {
        cps[text_seen] = zr_u32le_at(out + off + sizeof(zr_ev_record_header_t) + 0u);
      }
      text_seen++;
    }

    off += (size_t)rec_size;
  }

  ZR_ASSERT_EQ_U32(text_seen, 2u);
  ZR_ASSERT_EQ_U32(cps[0], 0x20ACu);
  ZR_ASSERT_EQ_U32(cps[1], 0xFFFDu);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_buffers_split_4byte_utf8_prefix) {
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
    U+1F600 (grinning face) is a 4-byte UTF-8 sequence.
    Prefix parsing must buffer an incomplete prefix across reads instead of
    emitting U+FFFD replacement scalars.
  */
  const uint8_t b0b1[] = {0xF0u, 0x9Fu};
  const uint8_t b2b3[] = {0x98u, 0x80u};

  mock_plat_set_read_max(2u);
  ZR_ASSERT_EQ_U32(mock_plat_push_input(b0b1, sizeof(b0b1)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));
  ZR_ASSERT_EQ_U32(engine_poll_events(e, 0, out, (int)sizeof(out)), 0);

  ZR_ASSERT_EQ_U32(mock_plat_push_input(b2b3, sizeof(b2b3)), ZR_OK);

  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  const uint32_t event_count = zr_u32le_at(out + 12u);
  ZR_ASSERT_TRUE(event_count >= 1u);

  size_t off = sizeof(zr_evbatch_header_t);
  uint32_t saw_grinning = 0u;

  for (uint32_t i = 0u; i < event_count; i++) {
    ZR_ASSERT_TRUE((off + sizeof(zr_ev_record_header_t)) <= (size_t)n);

    const uint32_t rec_type = zr_u32le_at(out + off + 0u);
    const uint32_t rec_size = zr_u32le_at(out + off + 4u);
    ZR_ASSERT_TRUE(rec_size >= (uint32_t)sizeof(zr_ev_record_header_t));
    ZR_ASSERT_TRUE((off + (size_t)rec_size) <= (size_t)n);

    if (rec_type == (uint32_t)ZR_EV_TEXT) {
      ZR_ASSERT_TRUE(rec_size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_text_t)));
      const uint32_t cp = zr_u32le_at(out + off + sizeof(zr_ev_record_header_t) + 0u);
      if (cp == 0x1F600u) {
        saw_grinning++;
      }
    }

    off += (size_t)rec_size;
  }

  ZR_ASSERT_EQ_U32(saw_grinning, 1u);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_does_not_buffer_impossible_utf8_prefix) {
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
    E0 80 is an impossible UTF-8 prefix (E0 requires second byte A0..BF).
    Prefix parsing must not defer this input as "incomplete".
  */
  const uint8_t in[] = {0xE0u, 0x80u};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  const uint32_t event_count = zr_u32le_at(out + 12u);
  ZR_ASSERT_TRUE(event_count >= 2u);

  size_t off = sizeof(zr_evbatch_header_t);
  uint32_t text_seen = 0u;
  uint32_t cps[2] = {0u, 0u};

  for (uint32_t i = 0u; i < event_count; i++) {
    ZR_ASSERT_TRUE((off + sizeof(zr_ev_record_header_t)) <= (size_t)n);

    const uint32_t rec_type = zr_u32le_at(out + off + 0u);
    const uint32_t rec_size = zr_u32le_at(out + off + 4u);
    ZR_ASSERT_TRUE(rec_size >= (uint32_t)sizeof(zr_ev_record_header_t));
    ZR_ASSERT_TRUE((off + (size_t)rec_size) <= (size_t)n);

    if (rec_type == (uint32_t)ZR_EV_TEXT) {
      ZR_ASSERT_TRUE(rec_size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_text_t)));
      if (text_seen < 2u) {
        cps[text_seen] = zr_u32le_at(out + off + sizeof(zr_ev_record_header_t) + 0u);
      }
      text_seen++;
    }

    off += (size_t)rec_size;
  }

  ZR_ASSERT_EQ_U32(text_seen, 2u);
  ZR_ASSERT_EQ_U32(cps[0], 0xFFFDu);
  ZR_ASSERT_EQ_U32(cps[1], 0xFFFDu);

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

ZR_TEST_UNIT(engine_poll_events_parses_split_csi_arrow) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  /* ESC [ A split across reads must not generate spurious ESC/TEXT events. */
  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'A'};
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

ZR_TEST_UNIT(engine_poll_events_parses_split_csi_u_key) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in[] = {0x1Bu, (uint8_t)'[', (uint8_t)'9', (uint8_t)';', (uint8_t)'5', (uint8_t)'u'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[128];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)ZR_KEY_TAB);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 4u), (uint32_t)ZR_MOD_CTRL);

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
      0x1Bu, (uint8_t)'O', (uint8_t)'P', 0x1Bu, (uint8_t)'O', (uint8_t)'Q',
      0x1Bu, (uint8_t)'O', (uint8_t)'R', 0x1Bu, (uint8_t)'O', (uint8_t)'S',
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
      0x1Bu,        (uint8_t)'[', (uint8_t)'1', (uint8_t)'5', (uint8_t)'~', 0x1Bu,        (uint8_t)'[',
      (uint8_t)'1', (uint8_t)'7', (uint8_t)'~', 0x1Bu,        (uint8_t)'[', (uint8_t)'1', (uint8_t)'8',
      (uint8_t)'~', 0x1Bu,        (uint8_t)'[', (uint8_t)'1', (uint8_t)'9', (uint8_t)'~',
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
  const uint8_t in[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'<', (uint8_t)'0', (uint8_t)';',
                        (uint8_t)'1', (uint8_t)'0', (uint8_t)';', (uint8_t)'5', (uint8_t)'M',
                        0x1Bu,        (uint8_t)'[', (uint8_t)'<', (uint8_t)'0', (uint8_t)';',
                        (uint8_t)'1', (uint8_t)'0', (uint8_t)';', (uint8_t)'5', (uint8_t)'m'};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 2u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_MOUSE);
  const size_t off_payload0 = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload0 + 0u), 9u); /* x: 10 -> 9 */
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload0 + 4u), 4u); /* y: 5 -> 4 */
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
  const uint8_t in[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'<', (uint8_t)'6', (uint8_t)'4', (uint8_t)';',
                        (uint8_t)'1', (uint8_t)'0', (uint8_t)';', (uint8_t)'5', (uint8_t)'M'};
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
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 20u), 0u); /* wheel_x */
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 24u), 1u); /* wheel_y */

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
  const uint8_t in[] = {0x1Bu,        (uint8_t)'[', (uint8_t)'<', (uint8_t)'3', (uint8_t)'5', (uint8_t)';',
                        (uint8_t)'1', (uint8_t)'0', (uint8_t)';', (uint8_t)'5', (uint8_t)'M'};
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

ZR_TEST_UNIT(engine_poll_events_emits_bracketed_paste_as_single_event) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'0', (uint8_t)'~',
      (uint8_t)'h', (uint8_t)'e', (uint8_t)'l', (uint8_t)'l', (uint8_t)'o', 0x1Bu,
      (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'1', (uint8_t)'~',
  };
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_PASTE);

  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), 5u); /* byte_len */

  const size_t off_bytes = off_payload + sizeof(zr_ev_paste_t);
  ZR_ASSERT_TRUE(memcmp(out + off_bytes, "hello", 5u) == 0);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_does_not_parse_bracketed_paste_when_disabled_by_caps) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  plat_caps_t caps;
  memset(&caps, 0, sizeof(caps));
  caps.color_mode = PLAT_COLOR_MODE_RGB;
  caps.supports_mouse = 1u;
  caps.supports_bracketed_paste = 0u;
  caps.supports_focus_events = 0u;
  caps.supports_osc52 = 0u;
  caps.supports_sync_update = 0u;
  caps.supports_scroll_region = 1u;
  caps.supports_cursor_shape = 1u;
  caps.supports_output_wait_writable = 1u;
  caps._pad0[0] = 0u;
  caps._pad0[1] = 0u;
  caps._pad0[2] = 0u;
  caps.sgr_attrs_supported = 0xFFFFFFFFu;
  mock_plat_set_caps(caps);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'0', (uint8_t)'~',
      (uint8_t)'h', (uint8_t)'e', (uint8_t)'l', (uint8_t)'l', (uint8_t)'o', 0x1Bu,
      (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'1', (uint8_t)'~',
  };
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[512];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_TRUE(!zr_batch_contains_record_type(out, (size_t)n, (uint32_t)ZR_EV_PASTE));

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_does_not_parse_bracketed_paste_when_disabled_by_config) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;
  cfg.plat.enable_bracketed_paste = 0u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'0', (uint8_t)'~',
      (uint8_t)'h', (uint8_t)'e', (uint8_t)'l', (uint8_t)'l', (uint8_t)'o', 0x1Bu,
      (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'1', (uint8_t)'~',
  };
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[512];
  memset(out, 0, sizeof(out));
  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_TRUE(!zr_batch_contains_record_type(out, (size_t)n, (uint32_t)ZR_EV_PASTE));

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_paste_payload_does_not_emit_key_events) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
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
    Paste payload may contain bytes that look like VT sequences (including ESC).
    While bracketed paste is active, they must be treated as payload bytes, not
    parsed into key/mouse events.
  */
  const uint8_t payload[] = {0x1Bu, (uint8_t)'[', (uint8_t)'A'};
  const uint8_t in[] = {
      0x1Bu,      (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'0', (uint8_t)'~', payload[0],   payload[1],
      payload[2], 0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'1', (uint8_t)'~',
  };
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_PASTE);

  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)sizeof(payload)); /* byte_len */

  const size_t off_bytes = off_payload + sizeof(zr_ev_paste_t);
  ZR_ASSERT_TRUE(memcmp(out + off_bytes, payload, sizeof(payload)) == 0);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_paste_then_arrow_emits_two_events) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in[] = {
      0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'0', (uint8_t)'~',
      (uint8_t)'h', (uint8_t)'i', 0x1Bu,        (uint8_t)'[', (uint8_t)'2', (uint8_t)'0',
      (uint8_t)'1', (uint8_t)'~', 0x1Bu,        (uint8_t)'[', (uint8_t)'A',
  };
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 2u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_PASTE);

  const uint32_t rec0_size = zr_u32le_at(out + off_rec0 + 4u);
  ZR_ASSERT_TRUE(rec0_size >= (uint32_t)(sizeof(zr_ev_record_header_t) + sizeof(zr_ev_paste_t)));
  ZR_ASSERT_TRUE(rec0_size <= (uint32_t)n);

  const size_t off_payload0 = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload0 + 0u), 2u); /* byte_len */
  const size_t off_bytes0 = off_payload0 + sizeof(zr_ev_paste_t);
  ZR_ASSERT_TRUE(memcmp(out + off_bytes0, "hi", 2u) == 0);

  const size_t off_rec1 = off_rec0 + (size_t)rec0_size;
  ZR_ASSERT_TRUE(off_rec1 + sizeof(zr_ev_record_header_t) <= (size_t)n);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec1 + 0u), (uint32_t)ZR_EV_KEY);

  const size_t off_payload1 = off_rec1 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload1 + 0u), (uint32_t)ZR_KEY_UP);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_paste_payload_includes_end_marker_prefix_bytes) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t payload[] = {
      (uint8_t)'A', 0x1Bu, (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'1', (uint8_t)'X', (uint8_t)'B',
  };
  const uint8_t in[] = {
      0x1Bu,      (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'0', (uint8_t)'~', payload[0],
      payload[1], payload[2],   payload[3],   payload[4],   payload[5],   payload[6],   payload[7],
      0x1Bu,      (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'1', (uint8_t)'~',
  };
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out[256];
  memset(out, 0, sizeof(out));

  const int n = engine_poll_events(e, 0, out, (int)sizeof(out));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out + 12u), 1u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_rec0 + 0u), (uint32_t)ZR_EV_PASTE);

  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out + off_payload + 0u), (uint32_t)sizeof(payload)); /* byte_len */

  const size_t off_bytes = off_payload + sizeof(zr_ev_paste_t);
  ZR_ASSERT_TRUE(memcmp(out + off_bytes, payload, sizeof(payload)) == 0);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_flushes_incomplete_paste_on_idle) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in[] = {
      0x1Bu, (uint8_t)'[', (uint8_t)'2', (uint8_t)'0', (uint8_t)'0', (uint8_t)'~', (uint8_t)'h', (uint8_t)'i',
  };
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out0[128];
  memset(out0, 0, sizeof(out0));
  ZR_ASSERT_TRUE(engine_poll_events(e, 0, out0, (int)sizeof(out0)) == 0);

  for (int i = 0; i < 3; i++) {
    uint8_t out_idle[128];
    memset(out_idle, 0, sizeof(out_idle));
    ZR_ASSERT_TRUE(engine_poll_events(e, 0, out_idle, (int)sizeof(out_idle)) == 0);
  }

  uint8_t out1[256];
  memset(out1, 0, sizeof(out1));
  const int n = engine_poll_events(e, 0, out1, (int)sizeof(out1));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + 12u), 1u);

  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_rec0 + 0u), (uint32_t)ZR_EV_PASTE);

  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_payload + 0u), 2u); /* byte_len */

  const size_t off_bytes = off_payload + sizeof(zr_ev_paste_t);
  ZR_ASSERT_TRUE(memcmp(out1 + off_bytes, "hi", 2u) == 0);

  engine_destroy(e);
}

ZR_TEST_UNIT(engine_poll_events_flushes_bare_esc_on_idle_poll) {
  mock_plat_reset();
  mock_plat_set_read_max(1u);
  mock_plat_set_size(10u, 4u);
  mock_plat_set_now_ms(1000u);

  zr_engine_config_t cfg = zr_engine_config_default();
  cfg.target_fps = 20u;
  cfg.limits.out_max_bytes_per_frame = 4096u;

  zr_engine_t* e = NULL;
  ZR_ASSERT_EQ_U32(engine_create(&e, &cfg), ZR_OK);
  ZR_ASSERT_TRUE(e != NULL);

  zr_drain_initial_resize(ctx, e);

  const uint8_t in[] = {0x1Bu};
  ZR_ASSERT_EQ_U32(mock_plat_push_input(in, sizeof(in)), ZR_OK);

  uint8_t out0[128];
  memset(out0, 0, sizeof(out0));
  ZR_ASSERT_TRUE(engine_poll_events(e, 0, out0, (int)sizeof(out0)) == 0);

  uint8_t out1[128];
  memset(out1, 0, sizeof(out1));
  const int n = engine_poll_events(e, 0, out1, (int)sizeof(out1));
  ZR_ASSERT_TRUE(n > 0);

  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + 12u), 1u);
  const size_t off_rec0 = sizeof(zr_evbatch_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_rec0 + 0u), (uint32_t)ZR_EV_KEY);
  const size_t off_payload = off_rec0 + sizeof(zr_ev_record_header_t);
  ZR_ASSERT_EQ_U32(zr_u32le_at(out1 + off_payload + 0u), (uint32_t)ZR_KEY_ESCAPE);

  engine_destroy(e);
}
