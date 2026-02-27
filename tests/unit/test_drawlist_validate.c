/*
  tests/unit/test_drawlist_validate.c — Unit tests for drawlist validation (v1).

  Why: Validates parser safety guarantees for the v1 command stream format:
  bounds checking, alignment validation, overlap detection, and opcode framing.
*/

#include "zr_test.h"

#include "core/zr_drawlist.h"

#include <string.h>

/* --- Byte-expansion macros for fixture construction --- */
#define ZR_U16LE(v) (uint8_t)((uint16_t)(v) & 0xFFu), (uint8_t)(((uint16_t)(v) >> 8u) & 0xFFu)
#define ZR_U32LE(v)                                                                                                    \
  (uint8_t)((uint32_t)(v) & 0xFFu), (uint8_t)(((uint32_t)(v) >> 8u) & 0xFFu),                                          \
      (uint8_t)(((uint32_t)(v) >> 16u) & 0xFFu), (uint8_t)(((uint32_t)(v) >> 24u) & 0xFFu)
#define ZR_I32LE(v) ZR_U32LE((uint32_t)(int32_t)(v))

#define ZR_DL_CMD_HDR(op, sz) ZR_U16LE((op)), ZR_U16LE(0u), ZR_U32LE((sz))

/*
 * Fixture 1: CLEAR + DEF_STRING("Hi") + DRAW_TEXT(string_id=1)
 */
const uint8_t zr_test_dl_fixture1[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(152u),
    ZR_U32LE(64u),
    ZR_U32LE(88u),
    ZR_U32LE(3u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),

    ZR_DL_CMD_HDR(ZR_DL_OP_DEF_STRING, 20u),
    ZR_U32LE(1u),
    ZR_U32LE(2u),
    (uint8_t)'H',
    (uint8_t)'i',
    (uint8_t)0u,
    (uint8_t)0u,

    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 60u),
    ZR_I32LE(1),
    ZR_I32LE(0),
    ZR_U32LE(1u),
    ZR_U32LE(0u),
    ZR_U32LE(2u),
    ZR_U32LE(0x01020304u),
    ZR_U32LE(0x0A0B0C0Du),
    ZR_U32LE(0x00000011u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
};

const size_t zr_test_dl_fixture1_len = sizeof(zr_test_dl_fixture1);

/*
 * Fixture 2: CLEAR + PUSH_CLIP + FILL_RECT (clipped) + POP_CLIP
 */
const uint8_t zr_test_dl_fixture2[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(156u),
    ZR_U32LE(64u),
    ZR_U32LE(92u),
    ZR_U32LE(4u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),

    ZR_DL_CMD_HDR(ZR_DL_OP_PUSH_CLIP, 24u),
    ZR_I32LE(1),
    ZR_I32LE(1),
    ZR_I32LE(2),
    ZR_I32LE(1),

    ZR_DL_CMD_HDR(ZR_DL_OP_FILL_RECT, 52u),
    ZR_I32LE(0),
    ZR_I32LE(0),
    ZR_I32LE(4),
    ZR_I32LE(3),
    ZR_U32LE(0x11111111u),
    ZR_U32LE(0x22222222u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_POP_CLIP, 8u),
};

const size_t zr_test_dl_fixture2_len = sizeof(zr_test_dl_fixture2);

/*
 * Fixture 3: CLEAR + DEF_STRING + DEF_BLOB(text-run) + DRAW_TEXT_RUN
 */
const uint8_t zr_test_dl_fixture3[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(220u),
    ZR_U32LE(64u),
    ZR_U32LE(156u),
    ZR_U32LE(4u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),

    ZR_DL_CMD_HDR(ZR_DL_OP_DEF_STRING, 24u),
    ZR_U32LE(1u),
    ZR_U32LE(6u),
    (uint8_t)'A',
    (uint8_t)'B',
    (uint8_t)'C',
    (uint8_t)'D',
    (uint8_t)'E',
    (uint8_t)'F',
    (uint8_t)0u,
    (uint8_t)0u,

    ZR_DL_CMD_HDR(ZR_DL_OP_DEF_BLOB, 100u),
    ZR_U32LE(1u),
    ZR_U32LE(84u),

    ZR_U32LE(2u),

    ZR_U32LE(1u),
    ZR_U32LE(2u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(1u),
    ZR_U32LE(0u),
    ZR_U32LE(3u),

    ZR_U32LE(3u),
    ZR_U32LE(4u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(1u),
    ZR_U32LE(3u),
    ZR_U32LE(3u),

    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT_RUN, 24u),
    ZR_I32LE(0),
    ZR_I32LE(0),
    ZR_U32LE(1u),
    ZR_U32LE(0u),
};

const size_t zr_test_dl_fixture3_len = sizeof(zr_test_dl_fixture3);

/*
 * Fixture 4: Wide glyph clipping test.
 */
const uint8_t zr_test_dl_fixture4[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(184u),
    ZR_U32LE(64u),
    ZR_U32LE(120u),
    ZR_U32LE(5u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),

    ZR_DL_CMD_HDR(ZR_DL_OP_PUSH_CLIP, 24u),
    ZR_I32LE(1),
    ZR_I32LE(0),
    ZR_I32LE(1),
    ZR_I32LE(1),

    ZR_DL_CMD_HDR(ZR_DL_OP_DEF_STRING, 20u),
    ZR_U32LE(1u),
    ZR_U32LE(4u),
    0xE7u,
    0x95u,
    0x8Cu,
    (uint8_t)'A',

    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 60u),
    ZR_I32LE(0),
    ZR_I32LE(0),
    ZR_U32LE(1u),
    ZR_U32LE(0u),
    ZR_U32LE(4u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_POP_CLIP, 8u),
};

const size_t zr_test_dl_fixture4_len = sizeof(zr_test_dl_fixture4);

/*
 * Fixture 5: CLEAR + SET_CURSOR
 */
const uint8_t zr_test_dl_fixture5_v2_cursor[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(92u),
    ZR_U32LE(64u),
    ZR_U32LE(28u),
    ZR_U32LE(2u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),

    ZR_DL_CMD_HDR(ZR_DL_OP_SET_CURSOR, 20u),
    ZR_I32LE(3),
    ZR_I32LE(4),
    (uint8_t)2u,
    (uint8_t)1u,
    (uint8_t)1u,
    (uint8_t)0u,
};

const size_t zr_test_dl_fixture5_v2_cursor_len = sizeof(zr_test_dl_fixture5_v2_cursor);

/*
 * Fixture 6: DRAW_TEXT slices share one persistent string.
 */
const uint8_t zr_test_dl_fixture6_v1_draw_text_slices[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(216u),
    ZR_U32LE(64u),
    ZR_U32LE(152u),
    ZR_U32LE(4u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),

    ZR_DL_CMD_HDR(ZR_DL_OP_DEF_STRING, 24u),
    ZR_U32LE(1u),
    ZR_U32LE(5u),
    (uint8_t)'H',
    (uint8_t)'e',
    (uint8_t)'l',
    (uint8_t)'l',
    (uint8_t)'o',
    (uint8_t)0u,
    (uint8_t)0u,
    (uint8_t)0u,

    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 60u),
    ZR_I32LE(0),
    ZR_I32LE(0),
    ZR_U32LE(1u),
    ZR_U32LE(0u),
    ZR_U32LE(2u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 60u),
    ZR_I32LE(2),
    ZR_I32LE(0),
    ZR_U32LE(1u),
    ZR_U32LE(2u),
    ZR_U32LE(3u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
};

const size_t zr_test_dl_fixture6_v1_draw_text_slices_len = sizeof(zr_test_dl_fixture6_v1_draw_text_slices);

/*
 * Fixture 7: Extended style + hyperlink refs through persistent strings.
 */
const uint8_t zr_test_dl_fixture7_v3_text_link[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(200u),
    ZR_U32LE(64u),
    ZR_U32LE(136u),
    ZR_U32LE(5u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),

    ZR_DL_CMD_HDR(ZR_DL_OP_DEF_STRING, 20u),
    ZR_U32LE(1u),
    ZR_U32LE(1u),
    (uint8_t)'X',
    (uint8_t)0u,
    (uint8_t)0u,
    (uint8_t)0u,

    ZR_DL_CMD_HDR(ZR_DL_OP_DEF_STRING, 28u),
    ZR_U32LE(2u),
    ZR_U32LE(9u),
    (uint8_t)'h',
    (uint8_t)'t',
    (uint8_t)'t',
    (uint8_t)'p',
    (uint8_t)'s',
    (uint8_t)':',
    (uint8_t)'/',
    (uint8_t)'/',
    (uint8_t)'x',
    (uint8_t)0u,
    (uint8_t)0u,
    (uint8_t)0u,

    ZR_DL_CMD_HDR(ZR_DL_OP_DEF_STRING, 20u),
    ZR_U32LE(3u),
    ZR_U32LE(3u),
    (uint8_t)'i',
    (uint8_t)'d',
    (uint8_t)'1',
    (uint8_t)0u,

    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 60u),
    ZR_I32LE(0),
    ZR_I32LE(0),
    ZR_U32LE(1u),
    ZR_U32LE(0u),
    ZR_U32LE(1u),
    ZR_U32LE(0x01020304u),
    ZR_U32LE(0u),
    ZR_U32LE(0x00000004u),
    ZR_U32LE(0x00000003u),
    ZR_U32LE(0x00010203u),
    ZR_U32LE(2u),
    ZR_U32LE(3u),
    ZR_U32LE(0u),
};

const size_t zr_test_dl_fixture7_v3_text_link_len = sizeof(zr_test_dl_fixture7_v3_text_link);

ZR_TEST_UNIT(drawlist_validate_fixtures_1_2_3_4_ok) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;

  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture2, zr_test_dl_fixture2_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture3, zr_test_dl_fixture3_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture4, zr_test_dl_fixture4_len, &lim, &v), ZR_OK);
}

ZR_TEST_UNIT(drawlist_validate_fixture5_cursor_ok) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture5_v2_cursor, zr_test_dl_fixture5_v2_cursor_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(v.hdr.version, 1u);
}

ZR_TEST_UNIT(drawlist_validate_fixture6_draw_text_slices_ok) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(
      zr_dl_validate(zr_test_dl_fixture6_v1_draw_text_slices, zr_test_dl_fixture6_v1_draw_text_slices_len, &lim, &v),
      ZR_OK);
}

ZR_TEST_UNIT(drawlist_validate_fixture7_text_link_ok) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture7_v3_text_link, zr_test_dl_fixture7_v3_text_link_len, &lim, &v),
                   ZR_OK);
  ZR_ASSERT_EQ_U32(v.hdr.version, 1u);
}

ZR_TEST_UNIT(drawlist_validate_rejects_nonzero_reserved_table_fields) {
  uint8_t buf[sizeof(zr_test_dl_fixture1)];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* Patch strings_count (header field 8) to non-zero. */
  buf[8u * 4u + 0u] = 1u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_FORMAT);
}

ZR_TEST_UNIT(drawlist_validate_rejects_alignment) {
  uint8_t buf[sizeof(zr_test_dl_fixture1)];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* Patch cmd_offset = 66 (unaligned) at field 4. */
  buf[4u * 4u + 0u] = 66u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_FORMAT);
}

ZR_TEST_UNIT(drawlist_validate_rejects_overlap) {
  uint8_t buf[sizeof(zr_test_dl_fixture1)];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* Patch cmd_offset = 32, overlapping header bytes [0..64). */
  buf[4u * 4u + 0u] = 32u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_FORMAT);
}

ZR_TEST_UNIT(drawlist_validate_unknown_opcode_is_unsupported) {
  uint8_t buf[sizeof(zr_test_dl_fixture1)];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* Patch second command opcode to 99: cmd stream starts at 64, CLEAR is 8 bytes. */
  const size_t second_cmd_off = 64u + 8u;
  buf[second_cmd_off + 0u] = 99u;
  buf[second_cmd_off + 1u] = 0u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_UNSUPPORTED);
}

ZR_TEST_UNIT(drawlist_validate_enforces_caps) {
  zr_dl_view_t v;

  zr_limits_t lim = zr_limits_default();
  lim.dl_max_cmds = 2u;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &lim, &v), ZR_ERR_LIMIT);

  lim = zr_limits_default();
  lim.dl_max_total_bytes = (uint32_t)zr_test_dl_fixture3_len - 4u;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture3, zr_test_dl_fixture3_len, &lim, &v), ZR_ERR_LIMIT);
}
