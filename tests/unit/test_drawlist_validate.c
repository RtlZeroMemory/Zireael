/*
  tests/unit/test_drawlist_validate.c — Unit tests for drawlist validation (v1 + v2).

  Why: Validates the drawlist parser's safety guarantees: bounds checking,
  alignment validation, overlap detection, and capability enforcement.
  Uses hand-crafted binary fixtures to test specific validation rules.

  Scenarios tested:
    - Valid drawlist fixtures pass validation
    - Empty table rule: strings_count=0 with non-zero offset rejected
    - Alignment rule: unaligned cmd_offset rejected
    - Overlap rule: overlapping sections (cmd stream / strings) rejected
    - Unknown opcode returns ZR_ERR_UNSUPPORTED
    - Capability limits enforced (max_cmds, max_text_run_segments)
    - v2 cursor command parsing/validation
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
 * Fixture 1: CLEAR + DRAW_TEXT("Hi")
 *
 * A minimal valid drawlist with two commands:
 *   - CLEAR (resets framebuffer)
 *   - DRAW_TEXT at (1,0) with text "Hi"
 *
 * Layout:
 *   [0..63]   Header (16 u32s)
 *   [64..119] Command stream (CLEAR 8B + DRAW_TEXT 48B = 56B, 2 cmds)
 *   [120..127] Strings span table (1 entry: offset=0, len=2)
 *   [128..131] Strings bytes ("Hi" + padding)
 */
const uint8_t zr_test_dl_fixture1[] = {
    /* zr_dl_header_t (16 u32) */
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(132u), /* magic/version/hdr/total */
    ZR_U32LE(64u),
    ZR_U32LE(56u),
    ZR_U32LE(2u), /* cmd offset/bytes/count */
    ZR_U32LE(120u),
    ZR_U32LE(1u),
    ZR_U32LE(128u),
    ZR_U32LE(4u), /* strings spans/count/bytes */
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u), /* blobs empty */
    ZR_U32LE(0u), /* reserved0 */

    /* cmd stream @ 64 */
    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),
    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 48u),
    ZR_I32LE(1),
    ZR_I32LE(0),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(2u), /* x,y,str,off,len */
    ZR_U32LE(0x01020304u),
    ZR_U32LE(0x0A0B0C0Du),
    ZR_U32LE(0x00000011u),
    ZR_U32LE(0u), /* style */
    ZR_U32LE(0u), /* cmd reserved0 */

    /* strings span table @ 120 */
    ZR_U32LE(0u),
    ZR_U32LE(2u),

    /* strings bytes @ 128 (len=4) */
    (uint8_t)'H',
    (uint8_t)'i',
    (uint8_t)0u,
    (uint8_t)0u,
};

const size_t zr_test_dl_fixture1_len = sizeof(zr_test_dl_fixture1);

/*
 * Fixture 2: CLEAR + PUSH_CLIP + FILL_RECT (clipped) + POP_CLIP
 *
 * Tests clipping stack with 4 commands. The FILL_RECT is clipped
 * to region (1,1)-(3,2) by the PUSH_CLIP.
 */
const uint8_t zr_test_dl_fixture2[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(144u),
    ZR_U32LE(64u),
    ZR_U32LE(80u),
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
    ZR_DL_CMD_HDR(ZR_DL_OP_FILL_RECT, 40u),
    ZR_I32LE(0),
    ZR_I32LE(0),
    ZR_I32LE(4),
    ZR_I32LE(3),
    ZR_U32LE(0x11111111u),
    ZR_U32LE(0x22222222u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_DL_CMD_HDR(ZR_DL_OP_POP_CLIP, 8u),
};

const size_t zr_test_dl_fixture2_len = sizeof(zr_test_dl_fixture2);

/*
 * Fixture 3: CLEAR + DRAW_TEXT_RUN with 2 segments
 *
 * Tests the DRAW_TEXT_RUN command which uses a blob to define multiple
 * styled text segments over a single string span ("ABCDEF").
 *   - Segment 0: style.fg=1, bytes [0..3) = "ABC"
 *   - Segment 1: style.fg=3, bytes [3..6) = "DEF"
 */
const uint8_t zr_test_dl_fixture3[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(180u),
    ZR_U32LE(64u),
    ZR_U32LE(32u),
    ZR_U32LE(2u),
    ZR_U32LE(96u),
    ZR_U32LE(1u),
    ZR_U32LE(104u),
    ZR_U32LE(8u),
    ZR_U32LE(112u),
    ZR_U32LE(1u),
    ZR_U32LE(120u),
    ZR_U32LE(60u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),
    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT_RUN, 24u),
    ZR_I32LE(0),
    ZR_I32LE(0),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    /* strings span table @ 96 */
    ZR_U32LE(0u),
    ZR_U32LE(6u),
    /* strings bytes @ 104 (len=8) */
    (uint8_t)'A',
    (uint8_t)'B',
    (uint8_t)'C',
    (uint8_t)'D',
    (uint8_t)'E',
    (uint8_t)'F',
    (uint8_t)0u,
    (uint8_t)0u,

    /* blobs span table @ 112 */
    ZR_U32LE(0u),
    ZR_U32LE(60u),

    /* blobs bytes @ 120 (len=60): u32 seg_count + segments */
    ZR_U32LE(2u),
    /* seg0: style + (string_index, byte_off, byte_len) */
    ZR_U32LE(1u),
    ZR_U32LE(2u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(3u),
    /* seg1 */
    ZR_U32LE(3u),
    ZR_U32LE(4u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(3u),
    ZR_U32LE(3u),
};

const size_t zr_test_dl_fixture3_len = sizeof(zr_test_dl_fixture3);

/*
 * Fixture 4: Wide glyph clipping test
 *
 * Tests that clipping does not affect cursor advancement for wide glyphs.
 * The clip only includes x==1, and the text is U+754C '界' (width=2) + 'A'.
 * The wide glyph at x=0 should still advance by 2, placing 'A' at x=2
 * (outside the clip).
 */
const uint8_t zr_test_dl_fixture4[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(164u),
    ZR_U32LE(64u),
    ZR_U32LE(88u),
    ZR_U32LE(4u),
    ZR_U32LE(152u),
    ZR_U32LE(1u),
    ZR_U32LE(160u),
    ZR_U32LE(4u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    /* cmd stream @ 64 */
    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),
    ZR_DL_CMD_HDR(ZR_DL_OP_PUSH_CLIP, 24u),
    ZR_I32LE(1),
    ZR_I32LE(0),
    ZR_I32LE(1),
    ZR_I32LE(1),
    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 48u),
    ZR_I32LE(0),
    ZR_I32LE(0),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(4u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_DL_CMD_HDR(ZR_DL_OP_POP_CLIP, 8u),

    /* strings span table @ 152 */
    ZR_U32LE(0u),
    ZR_U32LE(4u),

    /* strings bytes @ 160 (len=4): U+754C '界' + 'A' */
    0xE7u,
    0x95u,
    0x8Cu,
    (uint8_t)'A',
};

const size_t zr_test_dl_fixture4_len = sizeof(zr_test_dl_fixture4);

/*
 * Fixture 5 (v2): CLEAR + SET_CURSOR
 *
 * A minimal v2 drawlist with two commands:
 *   - CLEAR
 *   - SET_CURSOR to x=3, y=4, shape=bar, visible=1, blink=1
 */
const uint8_t zr_test_dl_fixture5_v2_cursor[] = {
    /* zr_dl_header_t (16 u32) */
    ZR_U32LE(0x4C44525Au), ZR_U32LE(2u), ZR_U32LE(64u), ZR_U32LE(92u), /* magic/version/hdr/total */
    ZR_U32LE(64u), ZR_U32LE(28u), ZR_U32LE(2u),                        /* cmd offset/bytes/count */
    ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u),            /* strings empty */
    ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u), ZR_U32LE(0u),            /* blobs empty */
    ZR_U32LE(0u),                                                      /* reserved0 */

    /* cmd stream @ 64 */
    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u), ZR_DL_CMD_HDR(ZR_DL_OP_SET_CURSOR, 20u), ZR_I32LE(3), ZR_I32LE(4), /* x,y */
    (uint8_t)2u, /* shape=bar */
    (uint8_t)1u, /* visible */
    (uint8_t)1u, /* blink */
    (uint8_t)0u, /* reserved0 */
};

const size_t zr_test_dl_fixture5_v2_cursor_len = sizeof(zr_test_dl_fixture5_v2_cursor);

/*
 * Fixture 6 (v1): DRAW_TEXT slices share one string
 *
 * A v1 drawlist that stores "Hello" once and renders it via two DRAW_TEXT
 * commands using byte slices:
 *   - bytes[0..2) = "He" at x=0
 *   - bytes[2..5) = "llo" at x=2
 */
const uint8_t zr_test_dl_fixture6_v1_draw_text_slices[] = {
    ZR_U32LE(0x4C44525Au),
    ZR_U32LE(1u),
    ZR_U32LE(64u),
    ZR_U32LE(184u),
    ZR_U32LE(64u),
    ZR_U32LE(104u),
    ZR_U32LE(3u),
    ZR_U32LE(168u),
    ZR_U32LE(1u),
    ZR_U32LE(176u),
    ZR_U32LE(8u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    ZR_DL_CMD_HDR(ZR_DL_OP_CLEAR, 8u),
    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 48u),
    ZR_I32LE(0),
    ZR_I32LE(0),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(2u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_DL_CMD_HDR(ZR_DL_OP_DRAW_TEXT, 48u),
    ZR_I32LE(2),
    ZR_I32LE(0),
    ZR_U32LE(0u),
    ZR_U32LE(2u),
    ZR_U32LE(3u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),
    ZR_U32LE(0u),

    /* strings span table @ 168 */
    ZR_U32LE(0u),
    ZR_U32LE(5u),

    /* strings bytes @ 176 (len=8) */
    (uint8_t)'H',
    (uint8_t)'e',
    (uint8_t)'l',
    (uint8_t)'l',
    (uint8_t)'o',
    (uint8_t)0u,
    (uint8_t)0u,
    (uint8_t)0u,
};

const size_t zr_test_dl_fixture6_v1_draw_text_slices_len = sizeof(zr_test_dl_fixture6_v1_draw_text_slices);

/*
 * Test: drawlist_validate_fixtures_1_2_3_4_ok
 *
 * Scenario: All hand-crafted test fixtures pass validation with default limits.
 *
 * Arrange: Default limits.
 * Act:     Validate each fixture.
 * Assert:  All return ZR_OK.
 */
ZR_TEST_UNIT(drawlist_validate_fixtures_1_2_3_4_ok) {
  /* --- Arrange --- */
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;

  /* --- Act & Assert: All fixtures pass validation --- */
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture2, zr_test_dl_fixture2_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture3, zr_test_dl_fixture3_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture4, zr_test_dl_fixture4_len, &lim, &v), ZR_OK);
}

ZR_TEST_UNIT(drawlist_validate_fixture5_v2_cursor_ok) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture5_v2_cursor, zr_test_dl_fixture5_v2_cursor_len, &lim, &v), ZR_OK);
  ZR_ASSERT_EQ_U32(v.hdr.version, 2u);
}

ZR_TEST_UNIT(drawlist_validate_v1_rejects_v2_cursor_opcode) {
  uint8_t buf[92];
  memcpy(buf, zr_test_dl_fixture5_v2_cursor, sizeof(buf));

  /* Patch version=1 at header field index 1 (in u32s). */
  buf[1 * 4 + 0] = 1u;
  buf[1 * 4 + 1] = 0u;
  buf[1 * 4 + 2] = 0u;
  buf[1 * 4 + 3] = 0u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_UNSUPPORTED);
}

ZR_TEST_UNIT(drawlist_validate_v2_cursor_rejects_bad_shape) {
  uint8_t buf[92];
  memcpy(buf, zr_test_dl_fixture5_v2_cursor, sizeof(buf));

  /* Patch shape byte (cmd payload byte offset: header 64 + CLEAR 8 + cmdhdr 8 + x/y 8 = 88). */
  buf[88] = (uint8_t)3u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_FORMAT);
}

ZR_TEST_UNIT(drawlist_validate_fixture6_v1_draw_text_slices_ok) {
  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;
  ZR_ASSERT_EQ_U32(
      zr_dl_validate(zr_test_dl_fixture6_v1_draw_text_slices, zr_test_dl_fixture6_v1_draw_text_slices_len, &lim, &v),
      ZR_OK);
}

/*
 * Test: drawlist_validate_rejects_empty_table_rule
 *
 * Scenario: "Empty table rule" — if strings_count=0, span/bytes offsets must
 *           also be zero. A count of 0 with non-zero offsets is invalid.
 *
 * Arrange: Copy fixture 1, patch strings_count to 0 (keep offsets non-zero).
 * Act:     Validate patched drawlist.
 * Assert:  Returns ZR_ERR_FORMAT.
 */
ZR_TEST_UNIT(drawlist_validate_rejects_empty_table_rule) {
  /* --- Arrange --- */
  uint8_t buf[132];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* Patch strings_count=0 at header field index 8 (in u32s). */
  buf[8 * 4 + 0] = 0u;
  buf[8 * 4 + 1] = 0u;
  buf[8 * 4 + 2] = 0u;
  buf[8 * 4 + 3] = 0u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;

  /* --- Act & Assert --- */
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_FORMAT);
}

/*
 * Test: drawlist_validate_rejects_alignment
 *
 * Scenario: Command stream offset must be 4-byte aligned. An unaligned
 *           offset is rejected.
 *
 * Arrange: Copy fixture 1, patch cmd_offset to 66 (not divisible by 4).
 * Act:     Validate patched drawlist.
 * Assert:  Returns ZR_ERR_FORMAT.
 */
ZR_TEST_UNIT(drawlist_validate_rejects_alignment) {
  /* --- Arrange --- */
  uint8_t buf[132];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* Patch cmd_offset = 66 (unaligned) at u32 field index 4. */
  buf[4 * 4 + 0] = 66u;
  buf[4 * 4 + 1] = 0u;
  buf[4 * 4 + 2] = 0u;
  buf[4 * 4 + 3] = 0u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;

  /* --- Act & Assert --- */
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_FORMAT);
}

/*
 * Test: drawlist_validate_rejects_overlap
 *
 * Scenario: Drawlist sections (header, cmd stream, strings) must not overlap.
 *           An offset that causes overlap is rejected.
 *
 * Arrange: Copy fixture 1, patch strings_span_offset to 80 (inside cmd stream).
 * Act:     Validate patched drawlist.
 * Assert:  Returns ZR_ERR_FORMAT.
 */
ZR_TEST_UNIT(drawlist_validate_rejects_overlap) {
  /* --- Arrange --- */
  uint8_t buf[132];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* Patch strings_span_offset = 80 (overlaps cmd stream) at u32 field index 7. */
  buf[7 * 4 + 0] = 80u;
  buf[7 * 4 + 1] = 0u;
  buf[7 * 4 + 2] = 0u;
  buf[7 * 4 + 3] = 0u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;

  /* --- Act & Assert --- */
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_FORMAT);
}

/*
 * Test: drawlist_validate_unknown_opcode_is_unsupported
 *
 * Scenario: Unknown/unsupported opcodes are rejected with ZR_ERR_UNSUPPORTED
 *           (distinct from ZR_ERR_FORMAT for structural issues).
 *
 * Arrange: Copy fixture 1, patch second command's opcode to 99 (undefined).
 * Act:     Validate patched drawlist.
 * Assert:  Returns ZR_ERR_UNSUPPORTED.
 */
ZR_TEST_UNIT(drawlist_validate_unknown_opcode_is_unsupported) {
  /* --- Arrange --- */
  uint8_t buf[132];
  memcpy(buf, zr_test_dl_fixture1, sizeof(buf));

  /* Patch opcode of 2nd command header. cmd stream starts at 64; first cmd is 8 bytes. */
  const size_t second_cmd_off = 64u + 8u;
  buf[second_cmd_off + 0] = 99u; /* Invalid opcode */
  buf[second_cmd_off + 1] = 0u;

  zr_limits_t lim = zr_limits_default();
  zr_dl_view_t v;

  /* --- Act & Assert --- */
  ZR_ASSERT_EQ_U32(zr_dl_validate(buf, sizeof(buf), &lim, &v), ZR_ERR_UNSUPPORTED);
}

/*
 * Test: drawlist_validate_enforces_caps
 *
 * Scenario: Capability limits (max_cmds, max_strings, max_blobs,
 *           max_text_run_segments) are enforced
 *           during validation. Exceeding limits returns ZR_ERR_LIMIT.
 *
 * Arrange: Set restrictive limits.
 * Act:     Validate fixtures that exceed the limits.
 * Assert:  All return ZR_ERR_LIMIT.
 */
ZR_TEST_UNIT(drawlist_validate_enforces_caps) {
  zr_dl_view_t v;

  /* --- Fixture 1 has 2 commands; limit to 1 --- */
  zr_limits_t lim = zr_limits_default();
  lim.dl_max_cmds = 1u;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture1, zr_test_dl_fixture1_len, &lim, &v), ZR_ERR_LIMIT);

  /* --- Fixture 3 has 2 text run segments; limit to 1 --- */
  lim = zr_limits_default();
  lim.dl_max_text_run_segments = 1u;
  ZR_ASSERT_EQ_U32(zr_dl_validate(zr_test_dl_fixture3, zr_test_dl_fixture3_len, &lim, &v), ZR_ERR_LIMIT);

  /* --- Fixture 1 patched to 2 strings; limit to 1 --- */
  uint8_t strings_over_cap[132];
  memcpy(strings_over_cap, zr_test_dl_fixture1, sizeof(strings_over_cap));
  strings_over_cap[8 * 4 + 0] = 2u;
  strings_over_cap[8 * 4 + 1] = 0u;
  strings_over_cap[8 * 4 + 2] = 0u;
  strings_over_cap[8 * 4 + 3] = 0u;

  lim = zr_limits_default();
  lim.dl_max_strings = 1u;
  ZR_ASSERT_EQ_U32(zr_dl_validate(strings_over_cap, sizeof(strings_over_cap), &lim, &v), ZR_ERR_LIMIT);

  /* --- Fixture 3 patched to 2 blobs; limit to 1 --- */
  uint8_t blobs_over_cap[180];
  memcpy(blobs_over_cap, zr_test_dl_fixture3, sizeof(blobs_over_cap));
  blobs_over_cap[12 * 4 + 0] = 2u;
  blobs_over_cap[12 * 4 + 1] = 0u;
  blobs_over_cap[12 * 4 + 2] = 0u;
  blobs_over_cap[12 * 4 + 3] = 0u;

  lim = zr_limits_default();
  lim.dl_max_blobs = 1u;
  ZR_ASSERT_EQ_U32(zr_dl_validate(blobs_over_cap, sizeof(blobs_over_cap), &lim, &v), ZR_ERR_LIMIT);
}
