#include "zfont.h"

static const uint8_t pixel_lut[4][16] = {
    /* bitnum = 1 */ {
        0, 255,                         // 0..1
        255,255,255,255,255,255,255,255,255,255,255,255
    },
    /* bitnum = 2 */ {
        0, 85, 170, 255,                // 0..3
        255,255,255,255,255,255,255,255,255,255,255,255
    },
    /* bitnum = 3 */ {
        0, 36, 73, 109, 146, 182, 219, 255,  // 0..7
        255,255,255,255,255,255,255,255
    },
    /* bitnum = 4 */ {
        0, 17, 34, 51, 68, 85, 102, 119,
        136, 153, 170, 187, 204, 221, 238, 255
    }
};

static inline uint8_t zf_pixel_fmt(uint8_t val, uint8_t bitnum)
{
    return pixel_lut[bitnum - 1][val];
}

static uint32_t zf_utf8_to_codepoint(const char **p)
{
    const uint8_t *s = (const uint8_t *)(*p);
    uint32_t codepoint = 0;

    if ( (s[0] & 0x80) == 0 ) {
        /* 1-byte sequence */
        codepoint = s[0] & 0x7F;
        (*p) += 1;
    } else if ( (s[0] & 0xE0) == 0xC0 ) {
        /* 2-byte sequence */
        codepoint = ((s[0] & 0x1F) << 6) |
                    (s[1] & 0x3F);
        (*p) += 2;
    } else if ( (s[0] & 0xF0) == 0xE0 ) {
        /* 3-byte sequence */
        codepoint = ((s[0] & 0x0F) << 12) |
                    ((s[1] & 0x3F) << 6) |
                    (s[2] & 0x3F);
        (*p) += 3;
    } else if ( (s[0] & 0xF8) == 0xF0 ) {
        /* 4-byte sequence */
        codepoint = ((s[0] & 0x07) << 18) |
                    ((s[1] & 0x3F) << 12) |
                    ((s[2] & 0x3F) << 6) |
                    (s[3] & 0x3F);
        (*p) += 4;
    }

    return codepoint;
}

int32_t zf_read_variable(const uint8_t **p)
{
    int32_t value = 0;

    while (true) {
        uint8_t n = **p;
        value = (value << 7) | (n & 0x7F);
        (*p)++;
        if ((n & 0x80) == 0) {
            break;
        }
    }

    return value;
}

static uint8_t zf_read_unsigned_bits(zf_bs_t *br, uint8_t bitnum)
{
    uint8_t start_bit_index = br->bit_index;
    uint8_t end_bit_index = start_bit_index + bitnum;

    uint8_t value = (*br->data >> start_bit_index);

    if (end_bit_index >= 8)
    {
        br->data++;
        br->byte_index++;

        value |= *br->data << (8 - start_bit_index);

        end_bit_index -= 8;
    }

    br->bit_index = end_bit_index;

    value &= (1 << bitnum) - 1;

    return value;
}

static int8_t zf_read_signed_bits(zf_bs_t *br, uint8_t bitnum)
{
    int8_t value = (int8_t)zf_read_unsigned_bits(br, bitnum);

    uint8_t shift = 8 - bitnum;
    value <<= shift;
    value >>= shift;

    return value;
}

static int8_t zf_read_rice(zf_bs_t *br, int8_t runlength_bitnum)
{
    uint16_t runlength_remainder = zf_read_unsigned_bits(br, runlength_bitnum);
    uint16_t runlength_quotient = 0;
    while (zf_read_unsigned_bits(br, 1)) {
        runlength_quotient++;
    }

    return (runlength_quotient << runlength_bitnum) + runlength_remainder;
}

static int8_t zf_read_unary(zf_bs_t *br)
{
    int8_t value = 0;
    while (zf_read_unsigned_bits(br, 1)) {
        value++;
    }
    return value;
}

void zf_log(const uint8_t *font)
{
    const zf_font_header_t *header = (const zf_font_header_t *)font;

    ZF_PRINT("");
    ZF_PRINT("Font size: %d", header->size);
    ZF_PRINT("Font ascent: %d", header->ascent);
    ZF_PRINT("Font descent: %d", header->descent);
    ZF_PRINT("Font bounding box: left=%d, bottom=%d, width=%d, height=%d",
        header->fbb_left, header->fbb_bottom,
        header->fbb_width, header->fbb_height);
    ZF_PRINT("Font glyph bounding box bitnum: left=%d, bottom=%d, width=%d, height=%d advance=%d",
        header->gbb_left_bitnum, header->gbb_bottom_bitnum,
        header->gbb_width_bitnum, header->gbb_height_bitnum,
        header->glyph_advance_bitnum
    );
    ZF_PRINT("Font glyph pixel bitnum: %d", header->glyph_pixel_bitnum);
    ZF_PRINT("Font glyph repeat black bitnum: %d", header->glyph_repeat_black_bitnum);
    ZF_PRINT("Font glyph repeat white bitnum: %d", header->glyph_repeat_white_bitnum);

}

int zf_get_font_height(const uint8_t *font)
{
    const zf_font_header_t *header = (const zf_font_header_t *)font;
    return header->ascent + header->descent;
}

int zf_get_glyph(const uint8_t *font, zf_codepoint_t codepoint, zf_glyph_ctx_t *g)
{
    const zf_font_header_t *header = (const zf_font_header_t *)font;

    const uint8_t *block_cur, *glyph=NULL, *glyph_end;
    int blk_l, blk_cp_min, blk_cp_max, glyph_code, glyph_l, glyph_oft;
    int value, black_value, white_value, run_length;

    int repeat_length, repeat_num, repeat_index;
    zf_bs_t repeat_bs;

    int pixel, total_pixels, target_pixel;

    bool block_not_continuous;

    black_value = 0;
    white_value = (1 << header->glyph_pixel_bitnum) - 1;

    block_cur = font + sizeof(zf_font_header_t);
    while (true) {
        blk_l = zf_read_variable(&block_cur);
        if (blk_l == 0) {
            break;
        }
        blk_cp_min = zf_read_variable(&block_cur);
        blk_cp_max = zf_read_variable(&block_cur);
        block_not_continuous = blk_cp_max & 0x1;
        blk_cp_max = blk_cp_max >> 1;

        ZF_LOG("Block: length=%d,  first_codepoint=0x%04X last_codepoint=0x%04X", blk_l, blk_cp_min, blk_cp_max);

        if ((codepoint >= blk_cp_min) && (codepoint <= blk_cp_max)) {
            /* block matched */
            glyph = block_cur;
            glyph_end = block_cur + blk_l;
            if (block_not_continuous) {
                glyph_code = blk_cp_min;
            } else {
                glyph_code = blk_cp_min - 1;
            }

            ZF_LOG("Glyph block data:");
            ZF_HEXDUMP(glyph, glyph_end - glyph);

            while (glyph < glyph_end) {
                if (block_not_continuous) {
                    glyph_oft = zf_read_variable(&glyph);
                } else {
                    glyph_oft = 1;
                }
                glyph_code += glyph_oft;
                glyph_l = zf_read_variable(&glyph);

                ZF_LOG("Glyph: oft=%d, length=%d, codepoint=0x%04X", glyph_oft, glyph_l, glyph_code);

                if (glyph_code == codepoint) {
                    /* glyph matched */
                    ZF_LOG("Glyph found for codepoint 0x%04X", codepoint);

                    ZF_LOG("Glyph data:");
                    ZF_HEXDUMP(glyph, glyph_l);

                    g->bs.data = glyph;
                    g->bs.size = glyph_l;
                    g->bs.byte_index = 0;
                    g->bs.bit_index = 0;

                    g->info.left = zf_read_signed_bits(&g->bs, header->gbb_left_bitnum);
                    g->info.bottom = zf_read_signed_bits(&g->bs, header->gbb_bottom_bitnum);
                    g->info.width = zf_read_unsigned_bits(&g->bs, header->gbb_width_bitnum);
                    g->info.height = zf_read_unsigned_bits(&g->bs, header->gbb_height_bitnum);
                    g->info.advance = zf_read_unsigned_bits(&g->bs, header->glyph_advance_bitnum);

                    g->glyph_pixel_bitnum = header->glyph_pixel_bitnum;
                    g->glyph_repeat_black_bitnum = header->glyph_repeat_black_bitnum;
                    g->glyph_repeat_white_bitnum = header->glyph_repeat_white_bitnum;
                    g->codepoint = codepoint;
                    g->font = header;

                    ZF_LOG("Glyph bitnum: left=%d, bottom=%d, width=%d, height=%d, advance=%d",
                        header->gbb_left_bitnum,
                        header->gbb_bottom_bitnum,
                        header->gbb_width_bitnum,
                        header->gbb_height_bitnum,
                        header->glyph_advance_bitnum
                    );
                    ZF_LOG("Glyph metrics: left=%d, bottom=%d, width=%d, height=%d, advance=%d",
                        g->info.left, g->info.bottom, g->info.width, g->info.height, g->info.advance);

                    ZF_LOG("BS: size=%d, byte_index=%d, bit_index=%d",
                        g->bs.size, g->bs.byte_index, g->bs.bit_index);


                    return ZF_OK;
                }

                glyph += glyph_l;
            }
        }

        block_cur += blk_l;
    }

    return ZF_ERR_GLYPH_NOT_FOUND;
}

int zf_get_pixel_from_glyph(zf_glyph_ctx_t *ctx, int x, int y)
{
    zf_glyph_ctx_t g = *ctx;

    const uint8_t *block_cur, *glyph=NULL, *glyph_end;
    int glyph_size, blk_l, blk_cp_cnt, blk_cp, glyph_code, glyph_l;
    int value, black_value, white_value, run_length;

    int repeat_length, repeat_num, repeat_index;
    zf_bs_t repeat_bs;

    int pixel, total_pixels, target_pixel;

    if ( (x < 0) || (x >= g.info.width) ||
            (y < 0) || (y >= g.info.height) ) {
        return 0;
    }

    black_value = 0;
    white_value = (1 << g.glyph_pixel_bitnum) - 1;
    pixel = 0;
    total_pixels = g.info.width * g.info.height;
    target_pixel = y * g.info.width + x;
    repeat_length = 0;
    repeat_num = 0;
    repeat_index = 0;
    while ((g.bs.byte_index < g.bs.size) && (pixel < total_pixels)) {
        value = zf_read_unsigned_bits(&g.bs, g.glyph_pixel_bitnum);
        if (value == black_value) {
            /* black */
            run_length = zf_read_rice(&g.bs, g.glyph_repeat_black_bitnum);
            if (run_length == 0) {
                /* repeat */
                repeat_length = zf_read_unary(&g.bs) + 2;
                repeat_num = zf_read_unary(&g.bs) + 2;

                repeat_index = repeat_length;
                repeat_bs = g.bs;
                continue;
            }
        } else if (value == white_value) {
            /* white */
            run_length = zf_read_rice(&g.bs, g.glyph_repeat_white_bitnum);
            run_length += 1;
        } else {
            run_length = 1;
            /* pixel value (only one pixel) */
        }

        pixel += run_length;
        ZF_LOG("pixel %d/%d, value: %d, run length: %d, repeat length: %d, repeat num: %d",
            pixel, total_pixels, value, run_length, repeat_length, repeat_num);

        if (target_pixel < pixel) {
            return zf_pixel_fmt(value, g.glyph_pixel_bitnum);
        }

        if (repeat_index) {
            repeat_index--;
            if (repeat_index == 0) {
                /* do repeat */
                repeat_num--;
                if (repeat_num) {
                    g.bs = repeat_bs;
                    repeat_index = repeat_length;
                } else {
                    repeat_length = 0;
                }
            }
        }
    }

    return 0;
}

int zf_get_pixel_from_glyph_with_box(zf_glyph_ctx_t *ctx, int xp, int yp, int x, int y, int w, int h)
{
    int xg, yg, wg, hg;
    int xl, yl;
    int xoft, yoft;

    xl = x;
    yl = y + ctx->font->ascent;

    xg = xl + ctx->info.left;
    yg = yl - ctx->info.bottom - ctx->info.height;
    wg = ctx->info.width;
    hg = ctx->info.height;

    // printf("Get pixel from glyph with box: xp=%d, yp=%d, xl=%d, yl=%d, glyph box=(%d,%d,%d,%d), target box=(%d,%d,%d,%d)\n",
    //     xp, yp,
    //     xl, yl,
    //     xg, yg, wg, hg,
    //     x, y, w, h
    // );

    if ((xp < xg) || (xp >= xg + wg) ||
        (yp < yg) || (yp >= yg + hg)) {
        return 0;
    }

    xoft = xp - xg;
    yoft = yp - yg;

    // printf("  Mapped to glyph pixel: xoft=%d, yoft=%d\n", xoft, yoft);

    return zf_get_pixel_from_glyph(ctx, xoft, yoft);
}


int zf_get_pixel(const uint8_t *font, zf_codepoint_t codepoint, int x, int y)
{
    zf_glyph_ctx_t g;
    int rc;

    rc = zf_get_glyph(font, codepoint, &g);
    if (rc != ZF_OK) {
        return rc;
    }

    return zf_get_pixel_from_glyph(&g, x, y);
}

int zf_get_glyph_by_text(const uint8_t *font, const char *text, zf_glyph_ctx_t *g, int g_size)
{
    const char *p = text;
    zf_codepoint_t codepoint;
    int count = 0;
    int rc;

    while (*p && (count < g_size)) {
        codepoint = zf_utf8_to_codepoint(&p);
        rc = zf_get_glyph(font, codepoint, &g[count]);

        printf("UTF-8 char to codepoint:%d, 0x%04X\n", rc, codepoint);
        if (rc != ZF_OK) {
            return rc;
        }
        count++;
    }

    return count;
}
