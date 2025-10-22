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

typedef struct {
    int8_t left;
    int8_t bottom;
    uint8_t width;
    uint8_t height;
    uint8_t advance;

    uint8_t *bitmap;
    uint8_t *bitmap_size;
} zf_glyph_t;

int zf_get_pixel(const char *font, uint16_t codepoint, int x, int y);
int zf_get_glyph(const char *font, uint16_t codepoint, zf_glyph_t *glyph);

#endif
