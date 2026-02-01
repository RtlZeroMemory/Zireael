/*
  tests/unit/test_drawlist_validate.c — Unit tests for drawlist v1 validation.
*/

#include "zr_test.h"

#include "core/zr_drawlist.h"

#include <string.h>

#define ZR_U16LE(v) (uint8_t)((uint16_t)(v)&0xFFu), (uint8_t)(((uint16_t)(v) >> 8u) & 0xFFu)
#define ZR_U32LE(v)                                                                    \
  (uint8_t)((uint32_t)(v)&0xFFu), (uint8_t)(((uint32_t)(v) >> 8u) & 0xFFu),             \
      (uint8_t)(((uint32_t)(v) >> 16u) & 0xFFu), (uint8_t)(((uint32_t)(v) >> 24u) & 0xFFu)
#define ZR_I32LE(v) ZR_U32LE((uint32_t)(int32_t)(v))

#define ZR_DL_CMD_HDR(op, sz) ZR_U16LE((op)), ZR_U16LE(0u), ZR_U32LE((sz))

/* Fixture 1: CLEAR + DRAW_TEXT("Hi"). */
const uint8_t zr_test_dl_fixture1[] = {
    /* zr_dl_header_t (16 u32) */
    ZR_U32LE(0x4C44525Au), ZR_U32LE(1u), ZR_U32LE(64u), ZR_U32LE(132u), /* magic/version/hdr/total */
    ZR_U32LE(64u), ZR_U32LE(56u), ZR_U32LE(2u),                         /* cmd offset/bytes/count */
    ZR_U32LE(120u), ZR_U32LE(1u), ZR_U32LE(128u), ZR_U32LE(4u),          /* strings spans/count/bytes */
    ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u),              /* blobs empty */
    ZR_U32LE(0u),                                                        /* reserved0 */

    /* cmd stream @ 64 */
    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),
    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 48u),
    ZR_I32LE(1), ZR_I32LE(0), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(2u), /* x,y,str,off,len */
    ZR_U32LE(0x01020304u), ZR_U32LE(0x0A0B0C0Du), ZR_U32LE(0x00000011u), ZR_U32LE(0u), /* style */
    ZR_U32LE(0u), /* cmd reserved0 */

    /* strings span table @ 120 */
    ZR_U32LE(0u), ZR_U32LE(2u),

    /* strings bytes @ 128 (len=4) */
    (uint8_t)'H', (uint8_t)'i', (uint8_t)0u, (uint8_t)0u,
};

const size_t zr_test_dl_fixture1_len = sizeof(zr_test_dl_fixture1);

/* Fixture 2: CLEAR + PUSH_CLIP + FILL_RECT (clipped) + POP_CLIP. */
const uint8_t zr_test_dl_fixture2[] = {
    ZR_U32LE(0x4C44525Au), ZR_U32LE(1u), ZR_U32LE(64u), ZR_U32LE(144u),
    ZR_U32LE(64u), ZR_U32LE(80u), ZR_U32LE(4u),
    ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u),
    ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),
    ZR_DL_CMD_HDR(ZR_DL_OP_PUSH_CLIP, 24u), ZR_I32LE(1), ZR_I32LE(1), ZR_I32LE(2), ZR_I32LE(1),
    ZR_DL_CMD_HDR(ZR_DL_OP_FILL_RECT, 40u),
    ZR_I32LE(0), ZR_I32LE(0), ZR_I32LE(4), ZR_I32LE(3),
    ZR_U32LE(0x11111111u), ZR_U32LE(0x22222222u), ZR_U32LE(0u), ZR_U32LE(0u),
    ZR_DL_CMD_HDR(ZR_DL_OP_POP_CLIP, 8u),
};

const size_t zr_test_dl_fixture2_len = sizeof(zr_test_dl_fixture2);

/* Fixture 3: CLEAR + DRAW_TEXT_RUN blob with 2 segments over one string span. */
const uint8_t zr_test_dl_fixture3[] = {
    ZR_U32LE(0x4C44525Au), ZR_U32LE(1u), ZR_U32LE(64u), ZR_U32LE(180u),
    ZR_U32LE(64u), ZR_U32LE(32u), ZR_U32LE(2u),
    ZR_U32LE(96u), ZR_U32LE(1u), ZR_U32LE(104u), ZR_U32LE(8u),
    ZR_U32LE(112u), ZR_U32LE(1u), ZR_U32LE(120u), ZR_U32LE(60u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),
    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT_RUN, 24u), ZR_I32LE(0), ZR_I32LE(0), ZR_U32LE(0u), ZR_U32LE(0u),

    /* strings span table @ 96 */
    ZR_U32LE(0u), ZR_U32LE(6u),
    /* strings bytes @ 104 (len=8) */
    (uint8_t)'A', (uint8_t)'B', (uint8_t)'C', (uint8_t)'D', (uint8_t)'E', (uint8_t)'F',
    (uint8_t)0u, (uint8_t)0u,

    /* blobs span table @ 112 */
    ZR_U32LE(0u), ZR_U32LE(60u),

    /* blobs bytes @ 120 (len=60): u32 seg_count + segments */
    ZR_U32LE(2u),
    /* seg0: style + (string_index, byte_off, byte_len) */
    ZR_U32LE(1u), ZR_U32LE(2u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(3u),
    /* seg1 */
    ZR_U32LE(3u), ZR_U32LE(4u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(3u), ZR_U32LE(3u),
};

const size_t zr_test_dl_fixture3_len = sizeof(zr_test_dl_fixture3);

/* Fixture 4: clip must not affect cursor advancement for wide glyphs. */
const uint8_t zr_test_dl_fixture4[] = {
    ZR_U32LE(0x4C44525Au), ZR_U32LE(1u), ZR_U32LE(64u), ZR_U32LE(164u),
    ZR_U32LE(64u), ZR_U32LE(88u), ZR_U32LE(4u),
    ZR_U32LE(152u), ZR_U32LE(1u), ZR_U32LE(160u), ZR_U32LE(4u),
    ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u),
    ZR_U32LE(0u),

    /* cmd stream @ 64 */
    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),
    ZR_DL_CMD_HDR(ZR_DL_OP_PUSH_CLIP, 24u), ZR_I32LE(1), ZR_I32LE(0), ZR_I32LE(1), ZR_I32LE(1),
    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 48u),
    ZR_I32LE(0), ZR_I32LE(0), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(4u),
    ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_DL_CMD_HDR(ZR_DL_OP_POP_CLIP, 8u),

    /* strings span table @ 152 */
    ZR_U32LE(0u), ZR_U32LE(4u),

    /* strings bytes @ 160 (len=4): U+754C '界' + 'A' */
    0xE7u, 0x95u, 0x8Cu, (uint8_t)'A',
};

const size_t zr_test_dl_fixture4_len = sizeof(zr_test_dl_fixture4);

ZR_TEST_UNIT(drawlist_validate_fixtures_1_2_3_4_ok) {
  zr_limits_t lim = zr_limits_default();

  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture2, zr_test_dl_fixture2_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture3, zr_test_dl_fixture3_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture4, zr_test_dl_fixture4_len, &lim, &v), ZR_OK);
}

ZR_TEST_UNIT(drawlist_validate_rejects_empty_table_rule) {
  uint8_t buf[132];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* Force strings_count=0 but keep non-zero offsets. header field index 8 in u32s. */
  buf[8 * 4 + 0] = 0u;
  buf[8 * 4 + 1] = 0u;
  buf[8 * 4 + 2] = 0u;
  buf[8 * 4 + 3] = 0u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_FORMAT);
}

ZR_TEST_UNIT(drawlist_validate_rejects_alignment) {
  uint8_t buf[132];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* cmd_offset = 66 (unaligned) at u32 field index 4. */
  buf[4 * 4 + 0] = 66u;
  buf[4 * 4 + 1] = 0u;
  buf[4 * 4 + 2] = 0u;
  buf[4 * 4 + 3] = 0u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_FORMAT);
}

ZR_TEST_UNIT(drawlist_validate_rejects_overlap) {
  uint8_t buf[132];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* strings_span_offset = 80 (inside cmd stream) at u32 field index 7. */
  buf[7 * 4 + 0] = 80u;
  buf[7 * 4 + 1] = 0u;
  buf[7 * 4 + 2] = 0u;
  buf[7 * 4 + 3] = 0u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_FORMAT);
}

ZR_TEST_UNIT(drawlist_validate_unknown_opcode_is_unsupported) {
  uint8_t buf[132];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* Patch opcode of 2nd command header in cmd stream. cmd stream starts at 64; first cmd is 8 bytes. */
  const size_t second_cmd_off = 64u + 8u;
  buf[second_cmd_off + 0] = 99u;
  buf[second_cmd_off + 1] = 0u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_UNSUPPORTED);
}

ZR_TEST_UNIT(drawlist_validate_enforces_caps) {
  zr_limits_t lim = zr_limits_default();
  lim.dl_max_cmds = 1u;

  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &lim, &v), ZR_ERR_LIMIT);

  lim = zr_limits_default();
  lim.dl_max_text_run_segments = 1u;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture3, zr_test_dl_fixture3_len, &lim, &v), ZR_ERR_LIMIT);
}
