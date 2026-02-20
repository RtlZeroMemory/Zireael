/*
  src/core/zr_blit.h — Sub-cell blitter core interface and shared helpers.

  Why: Provides deterministic pixel-to-cell conversion primitives used by
  DRAW_CANVAS execution without introducing platform dependencies in core.
*/

#ifndef ZR_CORE_ZR_BLIT_H_INCLUDED
#define ZR_CORE_ZR_BLIT_H_INCLUDED

#include "core/zr_framebuffer.h"

#include "util/zr_result.h"

#include "zr/zr_drawlist.h"
#include "zr/zr_terminal_caps.h"

#include <stddef.h>
#include <stdint.h>

/* Source pixel buffer view (RGBA8, row-major). */
typedef struct zr_blit_input_t {
  const uint8_t* pixels;
  uint16_t px_width;
  uint16_t px_height;
  uint16_t stride;
} zr_blit_input_t;

/* Capability snapshot for AUTO blitter selection. */
typedef struct zr_blit_caps_t {
  zr_terminal_id_t terminal_id;
  uint8_t is_dumb_terminal;
  uint8_t is_pipe_mode;
  uint8_t supports_unicode;
  uint8_t supports_quadrant;
  uint8_t supports_sextant;
  uint8_t supports_halfblock;
  uint8_t supports_braille;
  uint8_t include_braille_in_auto;
  uint8_t _pad0[3];
} zr_blit_caps_t;

typedef struct zr_blit_glyph_t {
  uint8_t bytes[4];
  uint8_t len;
  uint8_t _pad0[3];
} zr_blit_glyph_t;

enum {
  ZR_BLIT_RGBA_BYTES_PER_PIXEL = 4u,
  ZR_BLIT_ALPHA_THRESHOLD = 128u,
  ZR_BLIT_QUADRANT_GLYPHS = 16u,
  ZR_BLIT_SEXTANT_GLYPHS = 64u,
  ZR_BLIT_SEXTANT_SUBPIXELS = 6u,
  ZR_BLIT_QUADRANT_SUBPIXELS = 4u,
  ZR_BLIT_BRAILLE_SUBPIXELS = 8u,
  ZR_BLIT_HALFBLOCK_SUBPIXELS = 2u
};

enum {
  ZR_BLIT_HALF_GLYPH_SPACE = 0u,
  ZR_BLIT_HALF_GLYPH_UPPER = 1u,
  ZR_BLIT_HALF_GLYPH_LOWER = 2u,
  ZR_BLIT_HALF_GLYPH_FULL = 3u
};

/* AUTO selector and dispatch entry points. */
void zr_blit_caps_from_profile(const zr_terminal_profile_t* profile, zr_blit_caps_t* out_caps);
zr_result_t zr_blit_select(zr_blitter_t requested, const zr_blit_caps_t* caps, zr_blitter_t* out_effective);
zr_result_t zr_blit_dispatch(zr_fb_painter_t* painter, zr_rect_t dst_rect, const zr_blit_input_t* input,
                             zr_blitter_t requested, const zr_blit_caps_t* caps, zr_blitter_t* out_effective);

/* Shared deterministic math helpers. */
uint8_t zr_blit_alpha_is_opaque(uint8_t alpha);
uint32_t zr_blit_pack_rgb(uint8_t r, uint8_t g, uint8_t b);
uint32_t zr_blit_rgb_distance_sq(uint32_t rgb_a, uint32_t rgb_b);
uint32_t zr_blit_luma_bt709(uint32_t rgb);

/* Deterministic nearest-neighbor map for sub-cell sampling. */
uint32_t zr_blit_sample_axis(uint32_t sub_coord, uint32_t src_len, uint32_t dst_cells, uint32_t sub_per_cell);

/* Read one source pixel mapped from a destination sub-pixel coordinate. */
zr_result_t zr_blit_sample_subpixel(const zr_blit_input_t* input, uint32_t sub_x, uint32_t sub_y, uint32_t dst_cells_w,
                                    uint32_t dst_cells_h, uint32_t sub_w, uint32_t sub_h, uint8_t out_rgba[4]);

/* Write one single-width glyph cell through the framebuffer painter path. */
zr_result_t zr_blit_put_glyph(zr_fb_painter_t* painter, int32_t x, int32_t y, const zr_blit_glyph_t* glyph,
                              const zr_style_t* style);

/* Shared block lookup tables. */
extern const zr_blit_glyph_t zr_blit_halfblock_glyphs[4];
extern const zr_blit_glyph_t zr_blit_quadrant_glyphs[ZR_BLIT_QUADRANT_GLYPHS];
extern const zr_blit_glyph_t zr_blit_sextant_glyphs[ZR_BLIT_SEXTANT_GLYPHS];

/* Concrete blitter implementations. */
zr_result_t zr_blit_ascii(zr_fb_painter_t* painter, zr_rect_t dst_rect, const zr_blit_input_t* input);
zr_result_t zr_blit_halfblock(zr_fb_painter_t* painter, zr_rect_t dst_rect, const zr_blit_input_t* input);
zr_result_t zr_blit_quadrant(zr_fb_painter_t* painter, zr_rect_t dst_rect, const zr_blit_input_t* input);
zr_result_t zr_blit_sextant(zr_fb_painter_t* painter, zr_rect_t dst_rect, const zr_blit_input_t* input);
zr_result_t zr_blit_braille(zr_fb_painter_t* painter, zr_rect_t dst_rect, const zr_blit_input_t* input);

#endif /* ZR_CORE_ZR_BLIT_H_INCLUDED */
