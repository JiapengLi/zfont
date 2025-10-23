/*
 * Copyright (c) 2025 Jiapeng Li <mail@jiapeng.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __ZFONT_H__
#define __ZFONT_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "zfontcfg.h"

enum {
    ZF_OK = 0,
    ZF_ERR_VAR = -1,
    ZF_ERR_GLYPH_NOT_FOUND = -2,
};

typedef uint32_t zf_codepoint_t;

#ifndef   __PACKED
#define __PACKED                               __attribute__((packed, aligned(1)))
#endif

/* set pack */
typedef struct __PACKED {
    uint32_t size;
    int16_t cap_height;
    int16_t ascent;
    int16_t descent;

    /* font boundingbox */
    int16_t fbb_left;
    int16_t fbb_bottom;
    int16_t fbb_width;
    int16_t fbb_height;

    /* glyph boundingbox */
    uint8_t gbb_left_bitnum;
    uint8_t gbb_bottom_bitnum;
    uint8_t gbb_width_bitnum;
    uint8_t gbb_height_bitnum;

    uint8_t glyph_advance_bitnum;
    uint8_t glyph_pixel_bitnum;
    uint8_t glyph_repeat_black_bitnum;
    uint8_t glyph_repeat_white_bitnum;
} zf_font_header_t;

typedef struct {
    int8_t left;
    int8_t bottom;
    uint8_t width;
    uint8_t height;
    uint8_t advance;
} zf_glyph_t;

typedef struct {
    const uint8_t *data;
    int size;

    int byte_index;
    int bit_index;
} zf_bs_t;

typedef struct{
    zf_bs_t bs;
    const zf_font_header_t *font;

    zf_glyph_t info;

    uint8_t glyph_pixel_bitnum;
    uint8_t glyph_repeat_black_bitnum;
    uint8_t glyph_repeat_white_bitnum;

    zf_codepoint_t codepoint;
} zf_glyph_ctx_t;

int zf_get_font_height(const uint8_t *font);

int zf_get_pixel(const uint8_t *font, zf_codepoint_t codepoint, int x, int y);

int zf_get_glyph(const uint8_t *font, zf_codepoint_t codepoint, zf_glyph_ctx_t *g);
int zf_get_pixel_from_glyph(zf_glyph_ctx_t *ctx, int x, int y);

int zf_get_glyph_by_text(const uint8_t *font, const char *text, zf_glyph_ctx_t *g, int g_size);

int zf_get_pixel_from_glyph_with_box(zf_glyph_ctx_t *ctx, int xp, int yp, int x, int y, int w, int h);

void zf_log(const uint8_t *font);


#endif
